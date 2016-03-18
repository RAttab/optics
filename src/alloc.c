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
static const size_t alloc_max_len = 16384;

// [  0,     8] -> 1
// ]  8,   256] -> 16 = 256 / 16
// ]256, 16384] -> 6  = { 512, 1024, 2048, ... }
enum { alloc_classes = 1 + 16 + 7 };



// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed alloc
{
    struct slock lock;
    optics_off_t classes[alloc_classes];
};


// -----------------------------------------------------------------------------
// classes
// -----------------------------------------------------------------------------

static size_t alloc_class(size_t *len)
{
    assert(*len <= alloc_max_len);

    if (*len <= alloc_min_len) {
        *len = alloc_min_len;
        return 0;
    }

    // ]8, 256] we go by increments of 16 bytes.
    if (*len <= alloc_mid_len) {
        size_t class = ceil_div(*len, alloc_mid_inc);
        *len = class * alloc_mid_inc;

        assert(class < alloc_classes);
        return class;
    }

    // ]256, 2048] we go by powers of 2.
    *len = ceil_pow2(*len);
    size_t bits = ctz(*len) - ctz(alloc_mid_len);
    size_t class = bits + (alloc_mid_len / alloc_mid_inc);

    assert(class < alloc_classes);
    return class;
}


// -----------------------------------------------------------------------------
// fill
// -----------------------------------------------------------------------------

static optics_off_t alloc_fill_class(
        struct alloc *alloc, struct region *region, size_t len, size_t class)
{
    assert(!alloc->classes[class]);

    size_t slab = len * (len <= alloc_mid_len ? 256 : 16);
    size_t start = region_grow(region, slab);
    if (!start) return 0;

    const size_t nodes = slab / len;
    optics_off_t end = start + (nodes * len);
    assert(nodes > 2);

    for (optics_off_t node = start + len; node + len < end; node += len) {
        optics_off_t *pnode = region_ptr(region, node, sizeof(*pnode));
        if (!pnode) optics_abort();

        *pnode = node + len;
    }

    optics_off_t *plast = region_ptr(region, end - len, sizeof(*plast));
    {
        slock_lock(&alloc->lock);

        *plast = alloc->classes[class];
        alloc->classes[class] = start + len;

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
    optics_off_t *pclass = &alloc->classes[class];

    if (!*pclass) {
        slock_unlock(&alloc->lock);
        return alloc_fill_class(alloc, region, len, class);
    }

    optics_off_t off = *pclass;
    optics_off_t *pnode = region_ptr(region, off, len);

    optics_assert(*pclass != *pnode,
            "invalid alloc self-reference: %p", (void *) off);

    *pclass = *pnode;
    memset(pnode, 0, len);

    slock_unlock(&alloc->lock);
    return off;
}

void alloc_free(
        struct alloc *alloc, struct region *region, optics_off_t off, size_t len)
{
    slock_lock(&alloc->lock);

    optics_assert(len <= alloc_max_len,
            "invalid free len: off=%p, len=%lu", (void *) off, len);

    size_t class = alloc_class(&len);
    optics_off_t *pclass = &alloc->classes[class];

    optics_off_t *pnode = region_ptr(region, off, len);
    memset(pnode, 0xFF, len);
    *pnode = *pclass;
    *pclass = off;

    slock_unlock(&alloc->lock);
}
