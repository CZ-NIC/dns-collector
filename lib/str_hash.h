/*
 *	Hyper-super-meta-alt-control-shift extra fast str_len() and str_hash()
 *	routines
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"

/* An equivalent of the Intel's rol instruction.  */
#define	ROL(x, bits)	(((x) << (bits)) | ((x) >> (sizeof(uns)*8 - (bits))))

/* The following functions need str to be aligned to uns.  */
uns str_len_aligned(const char *str) __attribute__((const));
uns str_hash_aligned(const char *str) __attribute__((const));

#ifdef	CPU_ALLOW_UNALIGNED
#define	str_len(str)	str_len_aligned(str)
#define	str_hash(str)	str_hash_aligned(str)
#else
uns str_len(const char *str) __attribute__((const));
uns str_hash(const char *str) __attribute__((const));
#endif
