/*
 *	Hyper-super-meta-alt-control-shift extra fast str_len() and hash_*()
 *	routines
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 */

#ifndef _SHERLOCK_HASHFUNC_H
#define _SHERLOCK_HASHFUNC_H

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

static inline uns CONST hash_int(uns x) { return 6442450967*x; }

#endif
