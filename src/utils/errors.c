/* errors.c
   Rémi Attab (remi.attab@gmail.com), 01 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "errors.h"

// -----------------------------------------------------------------------------
// error
// -----------------------------------------------------------------------------

__thread struct optics_error optics_errno = { 0 };

void optics_abort()
{
    optics_perror(&optics_errno);
    optics_log_dump();
    abort();
}

void optics_error_exit()
{
    optics_perror(&optics_errno);
    optics_log_dump();
    exit(1);
}


size_t optics_strerror(struct optics_error *err, char *dest, size_t len)
{
    size_t i = 0;

    if (!err->errno_) {
        i = snprintf(dest, len, "<%lu> %s:%d: %s\n",
                tid(), err->file, err->line, err->msg);
    }
    else {
        i = snprintf(dest, len, "<%lu> %s:%d: %s - %s(%d)\n",
                tid(), err->file, err->line, err->msg,
                strerror(err->errno_), err->errno_);
    }

    if (err->backtrace_len > 0) {
        char **symbols = backtrace_symbols(err->backtrace, err->backtrace_len);
        for (int j = 0; j < err->backtrace_len; ++j) {
            i += snprintf(dest + i, len - i, "  {%d} %s\n", j, symbols[j]);
        }

        free(symbols);
    }

    return i;
}

static bool dump_to_syslog = false;

void optics_syslog()
{
    openlog(NULL, LOG_CONS | LOG_NDELAY | LOG_PID, LOG_USER);
    dump_to_syslog = true;
}

void optics_perror(struct optics_error *err)
{
    char buf[128 + optics_err_msg_cap + 80 * optics_err_backtrace_cap];
    size_t len = optics_strerror(err, buf, sizeof(buf));

    if (!dump_to_syslog) {
        if (write(2, buf, len) == -1)
            fprintf(stderr, "optics_perror failed: %s", strerror(errno));
    }
    else syslog(err->warning ? LOG_WARNING : LOG_ERR, "%s", buf);
}


static void optics_backtrace(struct optics_error *err)
{
    err->backtrace_len = backtrace(err->backtrace, optics_err_backtrace_cap);
    if (err->backtrace_len == -1)
        printf("unable to sample backtrace: %s", strerror(errno));
}


// -----------------------------------------------------------------------------
// fail
// -----------------------------------------------------------------------------

static bool abort_on_fail = 0;
void optics_dbg_abort_on_fail() { abort_on_fail = true; }

void optics_vfail(const char *file, int line, const char *fmt, ...)
{
    optics_errno = (struct optics_error)
        { .warning = false, .errno_ = 0, .file = file, .line = line };

    va_list args;
    va_start(args, fmt);
    (void) vsnprintf(optics_errno.msg, optics_err_msg_cap, fmt, args);
    va_end(args);

    optics_backtrace(&optics_errno);
    if (abort_on_fail) optics_abort();
}

void optics_vfail_errno(const char *file, int line, const char *fmt, ...)
{
    optics_errno = (struct optics_error)
        { .warning = false, .errno_ = errno, .file = file, .line = line };

    va_list args;
    va_start(args, fmt);
    (void) vsnprintf(optics_errno.msg, optics_err_msg_cap, fmt, args);
    va_end(args);

    optics_backtrace(&optics_errno);
    if (abort_on_fail) optics_abort();
}


// -----------------------------------------------------------------------------
// warn
// -----------------------------------------------------------------------------

static bool abort_on_warn = 0;
void optics_dbg_abort_on_warn() { abort_on_warn = true; }

void optics_vwarn_va(const char *file, int line, const char *fmt, va_list args)
{
    struct optics_error err = { .warning = true, .file = file, .line = line };
    (void) vsnprintf(err.msg, optics_err_msg_cap, fmt, args);
    optics_backtrace(&err);

    optics_perror(&err);
}


void optics_vwarn(const char *file, int line, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    optics_vwarn_va(file, line, fmt, args);

    va_end(args);
}

void optics_vwarn_errno(const char *file, int line, const char *fmt, ...)
{

    struct optics_error err =
        { .warning = true, .errno_ = errno, .file = file, .line = line };

    va_list args;
    va_start(args, fmt);
    (void) vsnprintf(err.msg, optics_err_msg_cap, fmt, args);
    va_end(args);

    optics_backtrace(&err);

    optics_perror(&err);
}
