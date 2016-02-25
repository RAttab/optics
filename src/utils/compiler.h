/* compiler.h
   Rémi Attab (remi.attab@gmail.com), 03 May 2014
   FreeBSD-style copyright and disclaimer apply

   Compiler related utilities and attributes.
*/

#pragma once

// -----------------------------------------------------------------------------
// attributes
// -----------------------------------------------------------------------------

#define optics_unused       __attribute__((unused))
#define optics_noreturn     __attribute__((noreturn))
#define optics_align(x)     __attribute__((aligned(x)))
#define optics_packed       __attribute__((__packed__))
#define optics_pure         __attribute__((pure))
#define optics_printf(x,y)  __attribute__((format(printf, x, y)))
#define optics_malloc       __attribute__((malloc))
#define optics_noinline     __attribute__((noinline))
#define optics_likely(x)    __builtin_expect(x, 1)
#define optics_unlikely(x)  __builtin_expect(x, 0)


// -----------------------------------------------------------------------------
// builtin
// -----------------------------------------------------------------------------

#define optics_unreachable() __builtin_unreachable()


// -----------------------------------------------------------------------------
// asm
// -----------------------------------------------------------------------------

#define optics_asm __asm__

#define optics_no_opt()         optics_asm volatile ("")
#define optics_no_opt_val(x)    optics_asm volatile ("" : "+r" (x))
#define optics_no_opt_clobber() optics_asm volatile ("" : : : "memory")
