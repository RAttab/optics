/* region_alloc.c
   Rémi Attab (remi.attab@gmail.com), 16 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

static const size_t alloc_min_len = 8;
static const size_t alloc_mid_inc = 16;
static const size_t alloc_mid_len = 256;
static const size_t alloc_max_len = 2048;

// [  0,    8] -> 1
// ]  8,  256] -> 16 = 256 / 16
// ]256, 2048] -> 3  = { 512, 1024, 2048 }
enum { alloc_classes = 20 };



// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct region_alloc_node
{
    atomic_off_t next;
};

struct region_alloc
{
    struct alloc_node classes[alloc_classes];
};

// -----------------------------------------------------------------------------
// classes
// -----------------------------------------------------------------------------

static size_t region_alloc_class(size_t *len)
{
    if (*len <= alloc_min_len) {
        *len = alloc_min_len;
        return 0;
    }

    // ]8, 256] we go by increments of 16 bytes.
    if (*len <= alloc_mid_len) {
        size_t class = ceil_div(*len, alloc_mid_inc);
        *len = class * alloc_mid_inc;
        return class;
    }

    // ]256, 2048] we go by powers of 2.
    *len = ceil_pow2(*len);
    size_t bits = ctz(*len) - ctz(alloc_mid_len);
    return bits + (alloc_mid_len / alloc_mid_inc);
}


// -----------------------------------------------------------------------------
// fill
// -----------------------------------------------------------------------------

static void * region_alloc_fill_class(
        struct region *region, struct alloc_node *class, size_t len)
{
    optics_off_t start = region_grow(region, page_len);
    optics_off_t end = start + (nodes * len);

    for (ilka_off_t node = start + len; node + len < end; node += len) {
        ilka_off_t *pnode = ilka_write_sys(alloc->region, node, sizeof(ilka_off_t));
        *pnode = alloc_tag(blocks, node + len);
    }

}


// -----------------------------------------------------------------------------
// alloc
// -----------------------------------------------------------------------------

