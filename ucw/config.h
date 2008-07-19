/*
 *	UCW Library -- Configuration-Dependent Definitions
 *
 *	(c) 1997--2007 Martin Mares <mj@ucw.cz>
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_CONFIG_H
#define _UCW_CONFIG_H

/* Configuration switches */

#include "autoconf.h"

/* Tell libc we're going to use all extensions available */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Types (based on standard C99 integers) */

#include <stddef.h>
#include <stdint.h>

typedef uint8_t byte;			/* exactly 8 bits, unsigned */
typedef uint8_t u8;			/* exactly 8 bits, unsigned */
typedef int8_t s8;			/* exactly 8 bits, signed */
typedef uint16_t u16;			/* exactly 16 bits, unsigned */
typedef int16_t s16;			/* exactly 16 bits, signed */
typedef uint32_t u32;			/* exactly 32 bits, unsigned */
typedef int32_t s32;			/* exactly 32 bits, signed */
typedef uint64_t u64;			/* exactly 64 bits, unsigned */
typedef int64_t s64;			/* exactly 64 bits, signed */

typedef unsigned int uns;		/* at least 32 bits */
typedef u32 ucw_time_t;			/* seconds since UNIX epoch */
typedef s64 timestamp_t;		/* milliseconds since UNIX epoch */

#ifdef CONFIG_LARGE_FILES		/* File positions */
typedef s64 ucw_off_t;
#else
typedef s32 ucw_off_t;
#endif

#endif
