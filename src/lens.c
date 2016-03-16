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

    enum optics_lens_type type;
    size_t lens_len;
    size_t name_len;

    atomic_off_t next;
    optics_off_t prev;
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
        struct optics *optics,
        enum optics_lens_type type,
        size_t lens_len,
        const char *name)
{
    size_t name_len = strnlen(name, optics_name_max_len) + 1;
    if (name_len == optics_name_max_len) return 0;

    size_t total_len = sizeof(struct lens) + name_len + lens_len;

    optics_off_t off = optics_alloc(optics, total_len);
    if (!off) return NULL;

    struct lens *lens = optics_ptr(optics, off, total_len);
    if (!lens) return NULL;

    lens->off = off;
    lens->type = type;
    lens->lens_len = lens_len;
    lens->name_len = name_len;
    memcpy(lens_name_ptr(lens), name, name_len - 1);

    return lens;
}

static void lens_free(struct optics *optics, struct lens *lens)
{
    size_t total_len = sizeof(struct lens) + lens->name_len + lens->lens_len;
    optics_free(optics, lens->off, total_len);
}

static bool lens_defer_free(struct optics *optics, struct lens *lens)
{
    size_t total_len = sizeof(struct lens) + lens->name_len + lens->lens_len;
    return optics_defer_free(optics, lens->off, total_len);
}

static struct lens *lens_ptr(struct optics *optics, optics_off_t off)
{
    struct lens *lens = optics_ptr(optics, off, sizeof(struct lens));
    if (!lens) return NULL;

    size_t total_len = sizeof(struct lens) + lens->name_len + lens->lens_len;
    return optics_ptr(optics, off, total_len);
}

static void * lens_sub_ptr(struct lens *lens, enum optics_lens_type type)
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

static enum optics_lens_type lens_type(struct lens *lens)
{
    return lens->type;
}

static const char * lens_name(struct lens *lens)
{
    size_t offset = sizeof(struct lens) + lens->lens_len;
    return (const char *) (((uint8_t *) lens) + offset);
}

static void lens_set_next(struct optics *optics, struct lens *lens, optics_off_t next)
{
    atomic_init(&lens->next, next);
    if (!next) return;

    lens_ptr(optics, next)->prev = lens->off;
}

static optics_off_t lens_next(struct lens *lens)
{
    return atomic_load(&lens->next);
}

static bool lens_is_dead(struct lens *lens)
{
    return atomic_load(&lens->dead);
}

static void lens_kill(struct optics *optics, struct lens *lens)
{
    atomic_store_explicit(&lens->dead, true, memory_order_release);

    optics_off_t next_off = atomic_load(&lens->next);

    if (next_off)
        lens_ptr(optics, next_off)->prev = lens->prev;

    if (lens->prev) {
        struct lens *prev = lens_ptr(optics, lens->prev);
        atomic_store(&prev->next, next_off);
    }
}


// -----------------------------------------------------------------------------
// implementations
// -----------------------------------------------------------------------------

#include "lens_counter.c"
#include "lens_gauge.c"
#include "lens_dist.c"
