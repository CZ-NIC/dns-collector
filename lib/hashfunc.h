/*
 *	UCW Library -- Hyper-super-meta-alt-control-shift extra fast
 *	str_len() and hash_*() routines
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_HASHFUNC_H
#define _UCW_HASHFUNC_H

#include "lib/lib.h"

/* An equivalent of the Intel's rol instruction.  */
#define	ROL(x, bits)	(((x) << (bits)) | ((x) >> (sizeof(uns)*8 - (bits))))

/* The following functions need str to be aligned to uns.  */
uns str_len_aligned(const byte *str) CONST;
uns hash_string_aligned(const byte *str) CONST;
uns hash_block_aligned(const byte *str, uns len) CONST;

#ifdef	CPU_ALLOW_UNALIGNED
#define	str_len(str)		str_len_aligned(str)
#define	hash_string(str)	hash_string_aligned(str)
#define	hash_block(str, len)	hash_block_aligned(str, len)
#else
uns str_len(const byte *str) CONST;
uns hash_string(const byte *str) CONST;
uns hash_block(const byte *str, uns len) CONST;
#endif

uns hash_string_nocase(const byte *str) CONST;

/*
 *  We hash integers by multiplying by a reasonably large prime with
 *  few ones in its binary form (to gave the compiler the possibility
 *  of using shifts and adds on architectures where multiplication
 *  instructions are slow).
 */
static inline uns CONST hash_u32(uns x) { return 0x01008041*x; }
static inline uns CONST hash_u64(u64 x) { return hash_u32((uns)x ^ (uns)(x >> 32)); }
static inline uns CONST hash_pointer(void *x) { return ((sizeof(x) <= 4) ? hash_u32((uns)(addr_int_t)x) : hash_u64((u64)(addr_int_t)x)); }

#endif
