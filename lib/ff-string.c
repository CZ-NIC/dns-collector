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
  ASSERT(l);
  byte *src;
  uns src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return NULL;
  do
    {
      uns cnt = MIN(l, src_len);
      for (uns i = cnt; i--;)
        {
	  byte v = *src++;
	  if (v == '\n')
	    {
              bdirect_read_commit(f, src);
	      goto exit;
	    }
	  *b++ = v;
	}
      if (unlikely(cnt == l))
        die("%s: Line too long", f->name);
      l -= cnt;
      bdirect_read_commit(f, src);
      src_len = bdirect_read_prepare(f, &src);
    }
  while (src_len);
exit:
  *b = 0;
  return b;
}

int
bgets_nodie(struct fastbuf *f, byte *b, uns l)
{
  ASSERT(l);
  byte *src, *start = b;
  uns src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return 0;
  do
    {
      uns cnt = MIN(l, src_len);
      for (uns i = cnt; i--;)
        {
	  byte v = *src++;
	  if (v == '\n')
	    {
	      bdirect_read_commit(f, src);
	      goto exit;
	    }
	  *b++ = v;
	}
      bdirect_read_commit(f, src);
      if (cnt == l)
        return -1;
      l -= cnt;
      src_len = bdirect_read_prepare(f, &src);
    }
  while (src_len);
exit:
  *b++ = 0;
  return b - start;
}

uns
bgets_bb(struct fastbuf *f, bb_t *bb)
{
  byte *src;
  uns src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return 0;
  bb_grow(bb, 1);
  byte *buf = bb->ptr;
  uns len = 0, buf_len = bb->len;
  do
    {
      uns cnt = MIN(src_len, buf_len);
      for (uns i = cnt; i--;)
        {
	  if (*src == '\n')
	    {
              bdirect_read_commit(f, src);
	      goto exit;
	    }
	  *buf++ = *src++;
	}
      len += cnt;
      if (cnt == src_len)
        {
	  bdirect_read_commit(f, src);
	  src_len = bdirect_read_prepare(f, &src);
	}
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
  while (src_len);
exit:
  *buf++ = 0;
  return buf - bb->ptr;
}

byte *
bgets_mp(struct mempool *mp, struct fastbuf *f)
{
  byte *src;
  uns src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return NULL;
#define BLOCK_SIZE 4096
  struct block {
    struct block *prev;
    byte data[BLOCK_SIZE];
  } *blocks = NULL;
  uns sum = 0, buf_len = BLOCK_SIZE;
  struct block *new_block = alloca(sizeof(struct block));
  byte *buf = new_block->data;
  do
    {
      uns cnt = MIN(src_len, buf_len);
      for (uns i = cnt; i--;)
        {
	  if (*src == '\n')
	    {
              bdirect_read_commit(f, src);
	      goto exit;
	    }
	  *buf++ = *src++;
	}
      if (cnt == src_len)
        {
	  bdirect_read_commit(f, src);
	  src_len = bdirect_read_prepare(f, &src);
	}
      else
	src_len -= cnt;
      if (cnt == buf_len)
        {
          new_block->prev = blocks;
          blocks = new_block;
          sum += buf_len = BLOCK_SIZE;
	  buf = new_block->data;
	}
      else
	buf_len -= cnt;
    }
  while (src_len);
exit: ; 
  uns len = buf - new_block->data;
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
  ASSERT(l);
  byte *src;
  uns src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return NULL;
  do
    {
      uns cnt = MIN(l, src_len);
      for (uns i = cnt; i--;)
        {
	  *b = *src++;
	  if (!*b)
	    {
              bdirect_read_commit(f, src);
	      return b;
	    }
	  b++;
	}
      if (unlikely(cnt == l))
        die("%s: Line too long", f->name);
      l -= cnt;
      bdirect_read_commit(f, src);
      src_len = bdirect_read_prepare(f, &src);
    }
  while (src_len);
  *b = 0;
  return b;
}
