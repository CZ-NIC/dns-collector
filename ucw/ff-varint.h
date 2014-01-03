/*
 *	UCW Library: Reading and writing Varints on Fastbuf Streams
 *
 *	(c) 2013 Tomas Valla <tom@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_FF_VARINT_H
#define _UCW_FF_VARINT_H

#include <ucw/fastbuf.h>
#include <ucw/varint.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define bget_varint_slow ucw_bget_varint_slow
#define bput_varint_slow ucw_bput_varint_slow
#endif

u64 bget_varint_slow(struct fastbuf *b, u64 repl);
void bput_varint_slow(struct fastbuf *b, u64 u);

/**
 * Reads u64 encoded as varint from the fastbuf b.
 * If the read is unsuccessful, returns repl.
 **/
static inline u64 bget_varint_repl(struct fastbuf *b, u64 repl)
{
	uns l;
	if (bavailr(b) >= 1) {
		l = varint_len(*b->bptr);
		if (bavailr(b) >= l) {
			varint_get(b->bptr, &repl);
			b->bptr += l;
			return repl;
		}
	}
	return bget_varint_slow(b, repl);
}

/**
 * Reads u64 encoded as varint from the fastbuf b.
 * If the read is unsuccessful, returns ~0LLU.
 **/
static inline u64 bget_varint(struct fastbuf *b)
{
	return bget_varint_repl(b, ~0LLU);
}

/** Writes u64 u encoded as varint to the fastbuf b. **/
static inline void bput_varint(struct fastbuf *b, u64 u)
{
	uns l = varint_space(u);
	if (bavailw(b) >= l)
		b->bptr += varint_put(b->bptr, u);
	else
		bput_varint_slow(b, u);
}

#endif
