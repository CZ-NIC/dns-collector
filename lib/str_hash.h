/*
 *	Hyper-super-meta-alt-control-shift extra fast str_len() and str_hash()
 *	routines
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"

/* An equivalent of the Intel's rol instruction.  */
#define	ROL(x, bits)	(((x) << (bits)) | ((x) >> (sizeof(uns)*8 - (bits))))

uns str_len(const char *str) __attribute__((const));
uns str_hash(const char *str) __attribute__((const));
