/* alloc.c
   Rémi Attab (remi.attab@gmail.com), 16 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

static const size_t alloc_min_len = 8;
static const size_t alloc_mid_inc = 16;
static const size_t alloc_mid_len = 256;
static const size_t alloc_max_len = 4096;

// [  0,    8] -> 1
// ]  8,  256] -> 16 = 256 / 16
// ]256, 4096] -> 4  = { 512, 1024, 2048, 4096 }
enum { alloc_classes = 1 + 16 + 4 };



// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed alloc_class
{
    optics_off_t alloc;
    atomic_off_t free;
};

struct optics_packed alloc
{
    struct slock lock;
    struct alloc_class classes[alloc_classes];
};


// -----------------------------------------------------------------------------
// classes
// -----------------------------------------------------------------------------

static size_t alloc_class(size_t *len)
{
    optics_assert(*len <= alloc_max_len,
            "invalid alloc len: %lu > %lu", *len, alloc_max_len);

    if (*len <= alloc_min_len) {
        *len = alloc_min_len;
        return 0;
    }

    // ]8, 256] we go by increments of 16 bytes.
    if (*len <= alloc_mid_len) {
        size_t class = ceil_div(*len, alloc_mid_inc);
        *len = class * alloc_mid_inc;

        optics_assert(class < alloc_classes,
                "invalid computed class: %lu < %d", class, alloc_classes);
        return class;
    }

    // ]256, 2048] we go by powers of 2.
    *len = ceil_pow2(*len);
    size_t bits = ctz(*len) - ctz(alloc_mid_len);
    size_t class = bits + (alloc_mid_len / alloc_mid_inc);

    optics_assert(class < alloc_classes,
            "invalid computed class: %lu < %d", class, alloc_classes);
    return class;
}


// -----------------------------------------------------------------------------
// fill
// -----------------------------------------------------------------------------

static optics_off_t alloc_fill_class(
        struct alloc *alloc, struct region *region, size_t len, size_t class)
{
    size_t slab = len * (len <= alloc_mid_len ? 256 : 16);
    size_t start = region_grow(region, slab);
    if (!start) return 0;

    const size_t nodes = slab / len;
    optics_off_t end = start + (nodes * len);
    optics_assert(nodes > 2, "invalid node count: %lu <= 2", nodes);

    for (optics_off_t node = start + len; node + len < end; node += len) {
        optics_off_t *pnode = region_ptr(region, node, sizeof(*pnode));
        if (!pnode) return 0;

        *pnode = node + len;
    }

    optics_off_t *plast = region_ptr(region, end - len, sizeof(*plast));
    if (!plast) return 0;

    {
        slock_lock(&alloc->lock);

        *plast = alloc->classes[class].alloc;
        alloc->classes[class].alloc = start + len;

        slock_unlock(&alloc->lock);
    }


    return start;
}


// -----------------------------------------------------------------------------
// alloc
// -----------------------------------------------------------------------------

void alloc_init(struct alloc *alloc)
{
    memset(alloc, 0, sizeof(*alloc));
}

optics_off_t alloc_alloc(struct alloc *alloc, struct region *region, size_t len)
{
    slock_lock(&alloc->lock);

    optics_assert(len <= alloc_max_len, "alloc size too big: %lu > %lu",
            len, alloc_max_len);

    size_t class = alloc_class(&len);
    optics_off_t *head = &alloc->classes[class].alloc;

    if (!*head) {
        // Synchronizes with alloc_free to make sure that the linked list
        // pointers are fully written before we make it available.
        *head = atomic_exchange_explicit(
                &alloc->classes[class].free, 0, memory_order_acquire);
    }

    if (!*head) {
        slock_unlock(&alloc->lock);
        return alloc_fill_class(alloc, region, len, class);
    }

    optics_off_t off = *head;
    optics_off_t *pnode = region_ptr(region, off, len);
    if (!pnode) goto fail;

    optics_assert(*head != *pnode,
            "invalid alloc self-reference: %p", (void *) off);

    *head = *pnode;
    memset(pnode, 0, len);

    slock_unlock(&alloc->lock);
    return off;

  fail:
    slock_unlock(&alloc->lock);
    return 0;
}

// Can't grab a lock since it might be called from the polling process. Instead,
// we enqueue the free block in a lock-free linked list which is periodically
// drained in its entirety back to the main allocation linked list.
void alloc_free(
        struct alloc *alloc, struct region *region, optics_off_t off, size_t len)
{
    optics_assert(len <= alloc_max_len,
            "invalid free len: off=%p, len=%lu", (void *) off, len);

    size_t class = alloc_class(&len);
    atomic_off_t *head = &alloc->classes[class].free;

    optics_off_t *pnode = region_ptr(region, off, len);
    if (!pnode) return;
    memset(pnode, 0xFF, len);

    // We never dereference the old head pointer so we don't have to synchronize
    // anything.
    optics_off_t old = atomic_load_explicit(head, memory_order_relaxed);
    do {
        *pnode = old;

    // Synchronizes with alloc_alloc to make sure that the pointer to the old
    // head is visible before we can read the node.
    } while (!atomic_compare_exchange_weak_explicit(head, &old, off,
                    memory_order_release, memory_order_relaxed));
}
