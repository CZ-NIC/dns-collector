/*
 *	Sherlock -- Configuration-Dependent Definitions
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_CONFIG_H
#define _SHERLOCK_CONFIG_H

/* Configuration switches */

#include "lib/autoconf.h"

#ifdef CONFIG_MAX_CONTEXTS
#define CONFIG_CONTEXTS
#endif

/* Version */

#define SHER_VER "3.2" SHERLOCK_VERSION_SUFFIX

/* Paths */

#define DEFAULT_CONFIG "cf/sherlock"

/* Types */

typedef unsigned char byte;		/* exactly 8 bits, unsigned */
typedef signed char sbyte;		/* exactly 8 bits, signed */
typedef unsigned short word;		/* exactly 16 bits, unsigned */
typedef short sword;			/* exactly 16 bits, signed */
typedef unsigned short u16;		/* exactly 16 bits, unsigned */
typedef short s16;			/* exactly 16 bits, signed */
typedef unsigned int u32;		/* exactly 32 bits, unsigned */
typedef int s32;			/* exactly 32 bits, signed */
typedef unsigned int uns;		/* at least 32 bits */
typedef unsigned long long int u64;	/* exactly 64 bits, unsigned */
typedef long long int s64;		/* exactly 64 bits, signed */
typedef unsigned long addr_int_t;	/* Both integer and address */
typedef unsigned int sh_time_t;		/* Timestamp */

#ifndef NULL
#define NULL (void *)0
#endif

typedef u32 oid_t;			/* Object ID */

/* Data types and functions for accessing file positions */

#ifdef CONFIG_LARGE_DB
typedef s64 sh_off_t;
#define BYTES_PER_O 5
#define BYTES_PER_P 8
#define bgeto(f) bget5(f)
#define bputo(f,l) bput5(f,l)
#define bgetp(f) bgetq(f)
#define bputp(f,l) bputq(f,l)
#define GET_O(p) GET_U40(p)
#define GET_P(p) GET_U64(p)
#define PUT_O(p,x) PUT_U40(p,x)
#define PUT_P(p,x) PUT_U64(p,x)
#else
typedef s32 sh_off_t;
#define BYTES_PER_O 4
#define BYTES_PER_P 4
#define bgeto(f) bgetl(f)
#define bputo(f,l) bputl(f,l)
#define bgetp(f) bgetl(f)
#define bputp(f,l) bputl(f,l)
#define GET_O(p) GET_U32(p)
#define GET_P(p) GET_U32(p)
#define PUT_O(p,x) PUT_U32(p,x)
#define PUT_P(p,x) PUT_U32(p,x)
#endif

/* Misc */

#ifdef __GNUC__

#undef inline
#define NONRET __attribute__((noreturn))
#define UNUSED __attribute__((unused))
#define CONSTRUCTOR __attribute__((constructor))
#define PACKED __attribute__((packed))
#define CONST __attribute__((const))
#define PURE __attribute__((const))
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#else
#error This program requires the GNU C compiler.
#endif

#endif
