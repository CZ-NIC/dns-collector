/*
 *	Sherlock Library -- Hash Functions
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_HASHFUNC_H
#define _SHERLOCK_HASHFUNC_H

uns hash_string(byte *x);
uns hash_string_nocase(byte *x);
static inline uns hash_int(uns x) { return 6442450967*x; }
uns hash_block(byte *x, uns len);

#endif
