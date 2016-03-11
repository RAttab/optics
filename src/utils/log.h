/* log.h
   Rémi Attab (remi.attab@gmail.com), 20 Sep 2015
   FreeBSD-style copyright and disclaimer apply

   Specialized logger used to debug data-races in code. Not meant as a general
   purpose logger.
*/

#pragma once

#include "compiler.h"

// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

/* #define OPTICS_LOG */
/* #define OPTICS_LOG_RING 1 */


// -----------------------------------------------------------------------------
// log
// -----------------------------------------------------------------------------

#ifndef OPTICS_LOG_RING
# define OPTICS_LOG_RING 0
#endif

#ifdef OPTICS_LOG
# define optics_log(t, ...) optics_log_impl(t, __VA_ARGS__)
#else
# define optics_log(t, ...) do { (void) t; } while (false)
#endif

void optics_log_impl(const char *title, const char *fmt, ...) optics_printf(2, 3);
void optics_log_dump();
