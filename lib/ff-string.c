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
	  byte v = *src++;
	  if (v == '\n')
	    {
              bdirect_read_commit(f, src);
	      goto exit;
	    }
	  *buf++ = v;
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
bgets_mp(struct fastbuf *f, struct mempool *mp)
{
  byte *src;
  uns src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return NULL;
#define BLOCK_SIZE (4096 - sizeof(void *))
  struct block {
    struct block *prev;
    byte data[BLOCK_SIZE];
  } *blocks = NULL;
  uns sum = 0, buf_len = BLOCK_SIZE, cnt;
  struct block first_block, *new_block = &first_block;
  byte *buf = new_block->data;
  do
    {
      cnt = MIN(src_len, buf_len);
      for (uns i = cnt; i--;)
        {
	  byte v = *src++;
	  if (v == '\n')
	    {
              bdirect_read_commit(f, src);
	      goto exit;
	    }
	  *buf++ = v;
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
	  new_block = alloca(sizeof(struct block));
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

void
bgets_stk_init(struct bgets_stk_struct *s)
{
  s->src_len = bdirect_read_prepare(s->f, &s->src);
  if (!s->src_len)
    {
      s->cur_buf = NULL;
      s->cur_len = 0;
    }
  else
    {
      s->old_buf = NULL;
      s->cur_len = 256;
    }
}

void
bgets_stk_step(struct bgets_stk_struct *s)
{
  byte *buf = s->cur_buf;
  uns buf_len = s->cur_len;
  if (s->old_buf)
    {
      memcpy( s->cur_buf, s->old_buf, s->old_len);
      buf += s->old_len;
      buf_len -= s->old_len;
    }
  do
    {
      uns cnt = MIN(s->src_len, buf_len);
      for (uns i = cnt; i--;)
        {
	  byte v = *s->src++;
	  if (v == '\n')
	    {
              bdirect_read_commit(s->f, s->src);
	      goto exit;
	    }
	  *buf++ = v;
	}
      if (cnt == s->src_len)
        {
	  bdirect_read_commit(s->f, s->src);
	  s->src_len = bdirect_read_prepare(s->f, &s->src);
	}
      else
	s->src_len -= cnt;
      if (cnt == buf_len)
        {
	  s->old_len = s->cur_len;
	  s->old_buf = s->cur_buf;
	  s->cur_len *= 2;
	  return;
	}
      else
	buf_len -= cnt;
    }
  while (s->src_len);
exit:
  *buf = 0;
  s->cur_len = 0;
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
