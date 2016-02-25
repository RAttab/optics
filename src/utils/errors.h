/* errors.h
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

// -----------------------------------------------------------------------------
// error
// -----------------------------------------------------------------------------

#define OPTICS_ERR_MSG_CAP 1024UL
#define OPTICS_ERR_BACKTRACE_CAP 256

struct optics_error
{
    const char *file;
    int line;

    int errno_; // errno can be a macro hence the underscore.
    char msg[OPTICS_ERR_MSG_CAP];

    void *backtrace[OPTICS_ERR_BACKTRACE_CAP];
    int backtrace_len;
};

extern __thread struct optics_error optics_err;

void optics_abort() optics_noreturn;
void optics_perror(struct optics_error *err);


// -----------------------------------------------------------------------------
// fail
// -----------------------------------------------------------------------------

void optics_dbg_abort_on_fail();

void optics_vfail(const char *file, int line, const char *fmt, ...)
    optics_printf(3, 4);

void optics_vfail_errno(const char *file, int line, const char *fmt, ...)
    optics_printf(3, 4);

#define optics_fail(...)                                \
    optics_vfail(__FILE__, __LINE__, __VA_ARGS__)

#define optics_fail_errno(...)                          \
    optics_vfail_errno(__FILE__, __LINE__, __VA_ARGS__)

// useful for pthread APIs which return the errno.
#define optics_fail_ierrno(err, ...)                            \
    do {                                                        \
        errno = err;                                            \
        optics_vfail_errno(__FILE__, __LINE__, __VA_ARGS__);    \
    } while (false)


// -----------------------------------------------------------------------------
// warn
// -----------------------------------------------------------------------------

void optics_dbg_abort_on_warn();

void optics_vwarn(const char *file, int line, const char *fmt, ...)
    optics_printf(3, 4);

void optics_vwarn_errno(const char *file, int line, const char *fmt, ...)
    optics_printf(3, 4);

#define optics_warn(...)                                \
    optics_vwarn(__FILE__, __LINE__, __VA_ARGS__)

#define optics_warn_errno(...)                          \
    optics_vwarn_errno(__FILE__, __LINE__, __VA_ARGS__)

// useful for pthread APIs which return the errno.
#define optics_warn_ierrno(err, ...)                            \
    do {                                                        \
        errno = err;                                            \
        optics_vwarn_errno(__FILE__, __LINE__, __VA_ARGS__);    \
    } while (false)


// -----------------------------------------------------------------------------
// assert
// -----------------------------------------------------------------------------

#define optics_assert(p, ...)                   \
    do {                                        \
        if (optics_likely(p)) break;            \
        optics_fail(__VA_ARGS__);               \
        optics_abort();                         \
    } while (0)

#define optics_todo(msg)                        \
    do {                                        \
        optics_fail("TODO: " msg);              \
        optics_abort();                         \
    } while (0)
