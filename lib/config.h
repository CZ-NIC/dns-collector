/*
 *	UCW Library -- Configuration-Dependent Definitions
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_CONFIG_H
#define _UCW_CONFIG_H

/* Configuration switches */

#include "lib/autoconf.h"

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

#ifdef CONFIG_LFS			/* File positions */
typedef s64 sh_off_t;
#else
typedef s32 sh_off_t;
#endif

#endif
