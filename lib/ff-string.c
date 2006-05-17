/*
 *	UCW Library -- Fast Buffered I/O: Strings
 *
 *	(c) 1997--2006 Martin Mares <mj@ucw.cz>
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/mempool.h"

byte *					/* Non-standard */
bgets(struct fastbuf *f, byte *b, uns l)
{
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k < 0)
    return NULL;
  while (b < e)
    {
      if (k == '\n' || k < 0)
	{
	  *b = 0;
	  return b;
	}
      *b++ = k;
      k = bgetc(f);
    }
  die("%s: Line too long", f->name);
}

int
bgets_nodie(struct fastbuf *f, byte *b, uns l)
{
  byte *start = b;
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k < 0)
    return 0;
  while (b < e)
    {
      if (k == '\n' || k < 0)
	{
	  *b++ = 0;
	  return b - start;
	}
      *b++ = k;
      k = bgetc(f);
    }
  return -1;
}

uns
bgets_bb(struct fastbuf *f, bb_t *bb)
{
  byte *buf = bb->ptr, *src;
  uns len = 0, buf_len = bb->len, src_len = bdirect_read_prepare(f, &src);
  while (src_len)
    {
      uns cnt = MIN(src_len, buf_len);
      for (uns i = cnt; i--;)
        {
	  if (*src == '\n')
	    {
	      *buf = 0;
	      return buf - bb->ptr;
	    }
	  *buf++ = *src++;
	}
      len += cnt;
      if (cnt == src_len)
	src_len = bdirect_read_prepare(f, &src);
      else
	src_len -= cnt;
      if (cnt == buf_len)
        {
	  bb_do_grow(bb, len + 1);
	  buf = bb->ptr + len;
	  buf_len = bb->len - len;
	}
      else
	buf_len -= cnt;
    }
  *buf = 0;
  return len;
}

byte *
bgets_mp(struct mempool *mp, struct fastbuf *f)
{
#define BLOCK_SIZE 4096
  struct block {
    struct block *prev;
    byte data[BLOCK_SIZE];
  } *blocks = NULL;
  uns sum = 0;
  for (;;)
    {
      struct block *new_block = alloca(sizeof(struct block));
      byte *b = new_block->data, *e = b + BLOCK_SIZE;
      while (b < e)
        {
	  int k = bgetc(f);
	  if (k == '\n' || k < 0)
	    {
	      uns len = b - new_block->data;
	      byte *result = mp_alloc(mp, sum + len + 1) + sum;
	      result[len] = 0;
	      memcpy(result, new_block->data, len);
	      while (blocks)
	        {
		  result -= BLOCK_SIZE;
		  memcpy(result, blocks->data, BLOCK_SIZE);
		  blocks = blocks->prev;
		}
	      return result;
	    }
	  *b++ = k;
	}
      new_block->prev = blocks;
      blocks = new_block;
      sum += BLOCK_SIZE;
    }
#undef BLOCK_SIZE
}

int
bgets_stk_step(struct fastbuf *f, byte *old_buf, byte *buf, uns len)
{
  if (old_buf)
    {
      len = len >> 1;
      memcpy(buf, old_buf, len);
      buf += len;
    }
  while (len--)
    {
      int k = bgetc(f);
      if (k == '\n' || k < 0)
	return *buf = 0;
      *buf++ = k;
    }
  return 1;
}

byte *
bgets0(struct fastbuf *f, byte *b, uns l)
{
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k < 0)
    return NULL;
  while (b < e)
    {
      if (k <= 0)
	{
	  *b = 0;
	  return b;
	}
      *b++ = k;
      k = bgetc(f);
    }
  die("%s: Line too long", f->name);
}
