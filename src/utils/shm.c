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
        return shm_err;
    }

    struct dirent state, *entry;
    enum shm_ret ret;

    while (true) {
        int err = readdir_r(dir, &state, &entry);
        if (err < 0) {
            optics_fail_ierrno(err, "unable to iterate over shm folder");
            goto fail_readdir;
        }

        if (!entry) break;
        if (entry->d_type != DT_REG) continue;
        if (memcmp(entry->d_name, shm_prefix, shm_prefix_len))
            continue;

        ret = cb(ctx, entry->d_name + shm_prefix_len);
        if (ret != shm_ok) goto done;
    }

  done:
    closedir(dir);
    return ret;

  fail_readdir:
    closedir(dir);
    return shm_err;
}
