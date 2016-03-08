/* shm.c
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// shm
// -----------------------------------------------------------------------------

int shm_foreach(void *ctx, shm_foreach_cb_t cb)
{
    static const char shm_dir[] = "/dev/shm";
    static const char shm_prefix[] = "optics.";
    const size_t shm_prefix_len = sizeof(shm_prefix) - 1;

    DIR *dir = opendir(shm_dir);
    if (!dir) {
        optics_fail_errno("unable to open '%s'", shm_dir);
        optics_abort();
    }

    // \todo: use re-entrant readdir_r
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;
        if (memcmp(entry->d_name, shm_prefix, shm_prefix_len))
            continue;

        int ret = cb(ctx, entry->d_name + shm_prefix_len);
        if (ret <= 0) return ret;
    }

    closedir(dir);
    return 1;
}
