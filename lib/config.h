/*
 *	Sherlock Library -- Configuration-Dependent Definitions
 *
 *	(c) 1997--2000 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef _SHERLOCK_CONFIG_H
#define _SHERLOCK_CONFIG_H

/* Version */

#define SHER_VER "1.3"

/* Features */

#define SHERLOCK_CONFIG_REF_WEIGHTS	/* Weighed references */
#define SHERLOCK_CONFIG_LARGE_DB	/* Support for DB files >4GB */
#define SHERLOCK_CONFIG_LFS		/* Large files on 32-bit systems */
#undef  SHERLOCK_CONFIG_LFS_LIBC	/* LFS supported directly by libc */

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

#ifndef NULL
#define NULL (void *)0
#endif

typedef u32 oid_t;			/* Object ID */

#ifdef SHERLOCK_CONFIG_LFS		/* off_t as passed to file functions */
typedef s64 sh_off_t;
#define BYTES_PER_FILE_POINTER 5
#else
typedef int sh_off_t;
#define BYTES_PER_FILE_POINTER 4
#endif

#ifdef SHERLOCK_CONFIG_LARGE_DB		/* off_t as present in database files */
typedef s64 sh_foff_t;
#else
typedef s32 sh_foff_t;
#endif

/* CPU characteristics */

#define CPU_LITTLE_ENDIAN
#undef CPU_BIG_ENDIAN
#define CPU_CAN_DO_UNALIGNED_WORDS
#define CPU_CAN_DO_UNALIGNED_LONGS
#define CPU_STRUCT_ALIGN 4

/* Misc */

#ifdef __GNUC__

#undef inline
#define NONRET __attribute__((noreturn))

#else

#define inline
#define NONRET

#endif

#endif
