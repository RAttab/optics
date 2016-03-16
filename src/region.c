/* region.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

static size_t region_len_default() { return 1024UL * page_len; }


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

static bool region_shm_name(const char *name, char *dest, size_t dest_len)
{
    int ret = snprintf(dest, dest_len, "optics.%s", name);
    if (ret > 0 && (size_t) ret < dest_len) return true;

    optics_fail("region name '%s' too long", name);
    return false;
}

static ssize_t region_file_len(int fd)
{
    struct stat stat;

    int ret = fstat(fd, &stat);
    if (ret == -1) {
        optics_fail_errno("unable to stat fd '%d'", fd);
        return -1;
    }

    return stat.st_size;
}


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct region_vma
{
    void *map;
    size_t len;

    struct region_vma *next;
};

struct region
{
    int fd;
    bool owned;
    char name[NAME_MAX];

    struct region_vma vma;
};


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

static bool region_unlink(const char *name)
{
    char shm_name[NAME_MAX];
    if (!region_shm_name(name, shm_name, sizeof(shm_name)))
        return false;

    if (shm_unlink(shm_name) == -1) {
        optics_fail_errno("unable to unlink region '%s'", shm_name);
        return false;
    }

    return true;
}

static bool region_create(struct region *region, const char *name)
{
    // Wipe any leftover regions if exists.
    (void) region_unlink(name);

    memset(region, 0, sizeof(*region));
    region->owned = true;

    if (!region_shm_name(name, region->name, sizeof(region->name)))
        goto fail_name;

    region->fd = shm_open(region->name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (region->fd == -1) {
        optics_fail_errno("unable to create region '%s'", name);
        goto fail_open;
    }

    region->vma.len = region_len_default();

    int ret = ftruncate(region->fd, region->vma.len);
    if (ret == -1) {
        optics_fail_errno("unable to resize region '%s' to '%lu'", name, region->vma.len);
        goto fail_truncate;
    }

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED;
    region->vma.map = mmap(0, region->vma.len, prot, flags, region->fd, 0);
    if (region->vma.map == MAP_FAILED) {
        optics_fail_errno("unable to map region '%s' to '%lu'", name, region->vma.len);
        goto fail_vma;
    }

    return true;

    munmap(region->vma.map, region->vma.len);

  fail_vma:
  fail_truncate:
    close(region->fd);
    shm_unlink(region->name);

  fail_open:
  fail_name:
    memset(region, 0, sizeof(*region));
    return false;
}


static bool region_open(struct region *region, const char *name)
{
    memset(region, 0, sizeof(*region));
    region->owned = false;

    if (!region_shm_name(name, region->name, sizeof(region->name)))
        goto fail_name;

    region->fd = shm_open(region->name, O_RDWR, 0);
    if (region->fd == -1) {
        optics_fail_errno("unable to create region '%s'", name);
        goto fail_open;
    }


    ssize_t len = region_file_len(region->fd);
    if (len < 0) {
        optics_fail_errno("unable to query length of region '%s'", name);
        goto fail_len;
    }
    region->vma.len = len;

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED;
    region->vma.map = mmap(0, region->vma.len, prot, flags, region->fd, 0);
    if (region->vma.map == MAP_FAILED) {
        optics_fail_errno("unable to map region '%s' to '%lu'", name, region->vma.len);
        goto fail_vma;
    }

    return true;

    munmap(region->vma.map, region->vma.len);

  fail_vma:
  fail_len:
    close(region->fd);

  fail_open:
  fail_name:
    memset(region, 0, sizeof(*region));
    return false;
}


static bool region_close(struct region *region)
{
    struct region_vma *node = &region->vma;
    while (node) {
        if (munmap(node->map, node->len) == -1) {
            optics_fail_errno("unable to unmap region '%s': {%p, %lu}",
                    region->name, (void *) node->map, node->len);
            return false;
        }

        struct region_vma *next = node->next;
        if (node != &region->vma) free(node);
        node = next;
    }

    if (close(region->fd) == -1) {
        optics_fail_errno("unable to close region '%s'", region->name);
        return false;
    }

    if (region->owned) {
        if (shm_unlink(region->name) == -1) {
            optics_fail_errno("unable to unlink region '%s'", region->name);
            return false;
        }
    }

    return true;
}


// -----------------------------------------------------------------------------
// vma
// -----------------------------------------------------------------------------

static size_t region_len(struct region *region)
{
    return region->vma.len;
}

static bool region_grow(struct region *region, size_t len)
{
    if (len <= region->vma.len) {
        optics_fail("invalid grow length: %lu <= %lu", len, region->vma.len);
        return 0;
    }

    int ret = ftruncate(region->fd, len);
    if (ret == -1) {
        optics_fail_errno("unable to resize region '%s' to '%lu'", region->name, len);
        goto fail_trunc;
    }

    // We remap the entire file into memory while keeping the old mapping
    // around. This is basically a hack to keep our existing pointer valid
    // without fragmenting the memory region which would slow down calls to
    // region_vma_access.

    struct region_vma *old = malloc(sizeof(*old));
    *old = region->vma;
    region->vma.len = len;

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED;
    region->vma.map = mmap(0, region->vma.len, prot, flags, region->fd, 0);
    if (region->vma.map == MAP_FAILED) {
        optics_fail_errno("unable to map region '%s' to '%lu'", region->name, region->vma.len);
        goto fail_mmap;
    }

    region->vma.next = old;
    return true;

  fail_mmap:
    free(old);

  fail_trunc:
    return false;
}

static void * region_ptr(struct region *region, optics_off_t off, size_t len)
{
    if (off + len > region->vma.len) {
        optics_fail("out-of-region access in region '%s' at '%p' with len '%lu'",
                region->name, (void *) off, len);
        return NULL;
    }

    return ((uint8_t *) region->vma.map) + off;
}
