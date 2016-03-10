/* lens.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens
{
    optics_off_t off;

    enum lens_type type;
    size_t lens_len;
    size_t name_len;

    optics_off_t next;
    atomic_uint_fast8_t dead;
};


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

static char * lens_name_ptr(struct lens *lens)
{
    size_t off = sizeof(struct lens) + lens->lens_len;
    return (char *) (((uint8_t *) lens) + off);
}

static struct lens *
lens_alloc(
        struct region *region,
        enum lens_type type,
        size_t lens_len,
        const char *name)
{
    size_t name_len = strnlen(name, optics_name_max_len) + 1;
    if (name_len == optics_name_max_len) return 0;

    size_t total_len = sizeof(struct lens) + name_len + lens_len;

    optics_off_t off = region_alloc(region, total_len);
    if (!off) return NULL;

    struct lens *lens = region_ptr(region, off, total_len);
    if (!lens) return NULL;

    lens->off = off;
    lens->type = type;
    lens->lens_len = lens_len;
    lens->name_len = name_len;
    memcpy(lens_name_ptr(lens), name, name_len - 1);

    return lens;
}

static void lens_free(struct region *region, struct lens *lens)
{
    size_t total_len = sizeof(struct lens) + lens->name_len + lens->lens_len;
    region_free(region, lens->off, total_len);
}

static struct lens *lens_ptr(struct region *region, optics_off_t off)
{
    struct lens *lens = region_ptr(region, off, sizeof(struct lens));
    if (!lens) return NULL;

    size_t total_len = sizeof(struct lens) + lens->name_len + lens->lens_len;
    return region_ptr(region, off, total_len);
}

static void * lens_sub_ptr(struct lens *lens, enum lens_type type)
{
    if (lens->type != type) {
        optics_fail("invalid lens type: %d != %d", lens->type, type);
        return NULL;
    }

    return ((uint8_t *) lens) + sizeof(struct lens);
}


// -----------------------------------------------------------------------------
// interface
// -----------------------------------------------------------------------------

static optics_off_t lens_off(struct lens *lens)
{
    return lens->off;
}

static enum lens_type lens_type(struct lens *lens)
{
    return lens->type;
}

static const char * lens_name(struct lens *lens)
{
    size_t offset = sizeof(struct lens) + lens->lens_len;
    return (const char *) (((uint8_t *) lens) + offset);
}

static void lens_kill(struct lens *lens)
{
    atomic_store_explicit(&lens->dead, true, memory_order_release);
}

static bool lens_is_dead(struct lens *lens)
{
    return atomic_load_explicit(&lens->dead, memory_order_acquire);
}

static optics_off_t lens_next(struct lens *lens)
{
    return lens->next;
}

static void lens_set_next(struct lens *lens, optics_off_t next)
{
    lens->next = next;
}

// -----------------------------------------------------------------------------
// implementations
// -----------------------------------------------------------------------------

#include "lens_counter.c"
#include "lens_gauge.c"
#include "lens_dist.c"
