/* errors.h
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "compiler.h"

#include <stddef.h>
#include <stdarg.h>


// -----------------------------------------------------------------------------
// dump
// -----------------------------------------------------------------------------

void optics_syslog();

// -----------------------------------------------------------------------------
// abort
// -----------------------------------------------------------------------------

void optics_abort() optics_noreturn;


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

void optics_vwarn_va(const char *file, int line, const char *fmt, va_list args);

#define optics_warn(...)                                \
    optics_vwarn(__FILE__, __LINE__, __VA_ARGS__)

#define optics_warn_errno(...)                          \
    optics_vwarn_errno(__FILE__, __LINE__, __VA_ARGS__)

#define optics_warn_va(fmt, args)                       \
    optics_vwarn_va(__FILE__, __LINE__, fmt, args);

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

#define optics_assert_alloc(p)                  \
    optics_assert((p) != NULL, "out-of-memory");
