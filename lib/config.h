/*
 *	Sherlock -- Configuration-Dependent Definitions
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_CONFIG_H
#define _SHERLOCK_CONFIG_H

/* Custom configuration */

#include "lib/custom.h"

/* Version */

#define SHER_VER "2.0" SHER_SUFFIX

/* CPU characteristics */

#define CPU_I386
#define CPU_LITTLE_ENDIAN
#undef CPU_BIG_ENDIAN
#define CPU_ALLOW_UNALIGNED
#define CPU_STRUCT_ALIGN 4
#undef CPU_64BIT_POINTERS

/* OS characteristics */

#define CONFIG_LINUX

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

#ifdef SHERLOCK_CONFIG_LARGE_DB
typedef s64 sh_off_t;
#define BYTES_PER_O 5
#define BYTES_PER_P 8
#define bgeto(f) bget5(f)
#define bputo(f,l) bput5(f,l)
#define bgetp(f) bgetq(f)
#define bputp(f,l) bputq(f,l)
#define GET_O(p) GET_U40(p)
#define GET_P(p) GET_U64(p)
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
#endif

/* Misc */

#ifdef __GNUC__

#undef inline
#define NONRET __attribute__((noreturn))
#define UNUSED __attribute__((unused))
#define CONSTRUCTOR __attribute__((constructor))
#define PACKED __attribute__((packed))

#else
#error This program requires the GNU C compiler.
#endif

#endif
