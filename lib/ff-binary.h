/*
 *	UCW Library -- Fast Buffered I/O on Binary Values
 *
 *	(c) 1997--2007 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_FF_BINARY_H
#define _UCW_FF_BINARY_H

#include "lib/fastbuf.h"
#include "lib/unaligned.h"

int bgetw_slow(struct fastbuf *f);
static inline int bgetw(struct fastbuf *f)
{
  int w;
  if (bavailr(f) >= 2)
    {
      w = GET_U16(f->bptr);
      f->bptr += 2;
      return w;
    }
  else
    return bgetw_slow(f);
}

u32 bgetl_slow(struct fastbuf *f);
static inline u32 bgetl(struct fastbuf *f)
{
  u32 l;
  if (bavailr(f) >= 4)
    {
      l = GET_U32(f->bptr);
      f->bptr += 4;
      return l;
    }
  else
    return bgetl_slow(f);
}

u64 bgetq_slow(struct fastbuf *f);
static inline u64 bgetq(struct fastbuf *f)
{
  u64 l;
  if (bavailr(f) >= 8)
    {
      l = GET_U64(f->bptr);
      f->bptr += 8;
      return l;
    }
  else
    return bgetq_slow(f);
}

u64 bget5_slow(struct fastbuf *f);
static inline u64 bget5(struct fastbuf *f)
{
  u64 l;
  if (bavailr(f) >= 5)
    {
      l = GET_U40(f->bptr);
      f->bptr += 5;
      return l;
    }
  else
    return bget5_slow(f);
}

void bputw_slow(struct fastbuf *f, uns w);
static inline void bputw(struct fastbuf *f, uns w)
{
  if (bavailw(f) >= 2)
    {
      PUT_U16(f->bptr, w);
      f->bptr += 2;
    }
  else
    bputw_slow(f, w);
}

void bputl_slow(struct fastbuf *f, u32 l);
static inline void bputl(struct fastbuf *f, u32 l)
{
  if (bavailw(f) >= 4)
    {
      PUT_U32(f->bptr, l);
      f->bptr += 4;
    }
  else
    bputl_slow(f, l);
}

void bputq_slow(struct fastbuf *f, u64 l);
static inline void bputq(struct fastbuf *f, u64 l)
{
  if (bavailw(f) >= 8)
    {
      PUT_U64(f->bptr, l);
      f->bptr += 8;
    }
  else
    bputq_slow(f, l);
}

void bput5_slow(struct fastbuf *f, u64 l);
static inline void bput5(struct fastbuf *f, u64 l)
{
  if (bavailw(f) >= 5)
    {
      PUT_U40(f->bptr, l);
      f->bptr += 5;
    }
  else
    bput5_slow(f, l);
}

/* I/O on uintptr_t */

#ifdef CPU_64BIT_POINTERS
#define bputa(x,p) bputq(x,p)
#define bgeta(x) bgetq(x)
#else
#define bputa(x,p) bputl(x,p)
#define bgeta(x) bgetl(x)
#endif

#endif
