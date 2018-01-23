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

    size_t lens_len;
    size_t name_len;

    atomic_off_t next;
    optics_off_t prev;

    // Struct is packed so keep the int at the bottom to avoid alignment issues
    // (not that x86 cares all that much... I blame my OCD).
    enum optics_lens_type type;

    // Allign to a cache line to avoid alignment issues in the lens itself.
    uint8_t padding[20];
};

static_assert(sizeof(struct lens) % 64 == 0,
    "lens header should align to a cache line to avoid various alignment issues");


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

static size_t lens_total_len(struct lens *lens)
{
    return sizeof(*lens) + lens->name_len + lens->lens_len;
}

static void lens_free(struct optics *optics, struct lens *lens)
{
    optics_free(optics, lens->off, lens_total_len(lens));
}

static bool lens_defer_free(struct optics *optics, struct lens *lens)
{
    return optics_defer_free(optics, lens->off, lens_total_len(lens));
}

static struct lens *lens_ptr(struct optics *optics, optics_off_t off)
{
    struct lens *lens = optics_ptr(optics, off, sizeof(*lens));
    if (optics_unlikely(!lens)) return NULL;

    return optics_ptr(optics, off, lens_total_len(lens));
}

static void * lens_sub_ptr(struct lens *lens, enum optics_lens_type type)
{
    if (optics_unlikely(lens->type != type)) {
        optics_fail("invalid lens type: %d != %d", lens->type, type);
        return NULL;
    }

    return ((uint8_t *) lens) + sizeof(struct lens);
}

static double lens_rescale(const struct optics_poll *poll, double value)
{
    return value / poll->elapsed;
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

static optics_off_t lens_next(struct lens *lens)
{
    // Synchronization for nodes is done while reading the head in
    // optics. Traversing the list doesn't require any synchronization since
    // deletion is done using the epoch mechanism which makes reading a stale
    // next pointer perfectly safe.
    return atomic_load_explicit(&lens->next, memory_order_relaxed);
}

// Should be called while holding the optics->lock which makes manipulating the
// prev pointers safe.
static void lens_set_next(struct optics *optics, struct lens *lens, optics_off_t next)
{
    atomic_init(&lens->next, next);
    if (!next) return;

    struct lens* next_node = lens_ptr(optics, next);
    optics_assert(!next_node->prev, "adding node not in a list: next=%p", (void *) next);
    lens_ptr(optics, next)->prev = lens->off;
}

// Should be called while holding the optics->lock which makes manipulating the
// prev pointers safe. See lens_next for details on memory ordering.
static void lens_kill(struct optics *optics, struct lens *lens)
{
    optics_off_t next_off = atomic_load(&lens->next);

    if (next_off) {
        struct lens *next = lens_ptr(optics, next_off);
        optics_assert(next->prev == lens->off, "corrupted lens list: %p != %p",
                (void *) next->prev, (void *) lens->off);
        next->prev = lens->prev;
    }

    if (lens->prev) {
        struct lens *prev = lens_ptr(optics, lens->prev);
        optics_assert(prev->next == lens->off, "corrupted lens list: %p != %p",
                (void *) prev->next, (void *) lens->off);
        atomic_store_explicit(&prev->next, next_off, memory_order_relaxed);
    }
}


// -----------------------------------------------------------------------------
// implementations
// -----------------------------------------------------------------------------

#include "lens_counter.c"
#include "lens_gauge.c"
#include "lens_dist.c"
#include "lens_histo.c"
#include "lens_streaming.c"
