/*
 *	UCW Library -- Fast Buffered I/O: Strings
 *
 *	(c) 1997--2006 Martin Mares <mj@ucw.cz>
 *	(c) 2006--2018 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>
#include <ucw/mempool.h>
#include <ucw/bbuf.h>

char *					/* Non-standard */
bgets(struct fastbuf *f, char *b, uint l)
{
  ASSERT(l);
  byte *src;
  uint src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return NULL;
  do
    {
      uint cnt = MIN(l, src_len);
      for (uint i = cnt; i--;)
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
        bthrow(f, "toolong", "%s: Line too long", f->name);
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
bgets_nodie(struct fastbuf *f, char *b, uint l)
{
  ASSERT(l);
  byte *src, *start = b;
  uint src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return 0;
  do
    {
      uint cnt = MIN(l, src_len);
      for (uint i = cnt; i--;)
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
  return b - (char *)start;
}

uint
bgets_bb(struct fastbuf *f, struct bb_t *bb, uint limit)
{
  ASSERT(limit);
  byte *src;
  uint src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return 0;
  bb_grow(bb, 1);
  byte *buf = bb->ptr;
  uint len = 0, buf_len = MIN(bb->len, limit);
  do
    {
      uint cnt = MIN(src_len, buf_len);
      for (uint i = cnt; i--;)
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
	  if (unlikely(len == limit))
            bthrow(f, "toolong", "%s: Line too long", f->name);
	  bb_do_grow(bb, len + 1);
	  buf = bb->ptr + len;
	  buf_len = MIN(bb->len, limit) - len;
	}
      else
	buf_len -= cnt;
    }
  while (src_len);
exit:
  *buf++ = 0;
  return buf - bb->ptr;
}

char *
bgets_mp(struct fastbuf *f, struct mempool *mp)
{
  byte *src;
  uint src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return NULL;
  byte *buf = mp_start_noalign(mp, 1);
  size_t buf_len = mp_avail(mp);
  do
    {
      uint cnt = MIN(src_len, buf_len);
      for (uint i = cnt; i--;)
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
	  buf = mp_spread(mp, buf, 1);
	  buf_len = mp_avail(mp) - (buf - (byte *)mp_ptr(mp));
	}
      else
	buf_len -= cnt;
    }
  while (src_len);
exit:
  buf = mp_spread(mp, buf, 1);
  *buf++ = 0;
  return mp_end(mp, buf);
}

char *
bgets0(struct fastbuf *f, char *b, uint l)
{
  ASSERT(l);
  byte *src;
  uint src_len = bdirect_read_prepare(f, &src);
  if (!src_len)
    return NULL;
  do
    {
      uint cnt = MIN(l, src_len);
      for (uint i = cnt; i--;)
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
        bthrow(f, "toolong", "%s: Line too long", f->name);
      l -= cnt;
      bdirect_read_commit(f, src);
      src_len = bdirect_read_prepare(f, &src);
    }
  while (src_len);
  *b = 0;
  return b;
}
