/*
 *	LiZaRd -- Reading and writing to a fastbuf
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/lizard.h"
#include "lib/bbuf.h"
#include "lib/fastbuf.h"
#include "lib/bucket.h"

#include <errno.h>

static uns liz_type;
static float liz_min_compr;

static bb_t bb_in, bb_out;

void
lizard_set_type(uns type, float min_compr)
{
  liz_type = type;
  liz_min_compr = min_compr;
}

int
lizard_bwrite(struct fastbuf *fb_out, byte *ptr_in, uns len_in)
{
  byte *ptr_out;
  uns len_out;
  uns type = liz_type;
  if (type == BUCKET_TYPE_V33_LIZARD && liz_min_compr)
  {
    uns est_out = len_in * LIZARD_MAX_MULTIPLY + LIZARD_MAX_ADD + 16;
    uns aval_out = bdirect_write_prepare(fb_out, &ptr_out);
    if (aval_out < est_out)
    {
      bb_grow(&bb_out, est_out);
      ptr_out = bb_out.ptr;
    }
    else
      ptr_out += 16;
    len_out = lizard_compress(ptr_in, len_in, ptr_out);
    if (len_out + 8 > len_in * liz_min_compr)
    {
      type = BUCKET_TYPE_V33;
      ptr_out = ptr_in;
      len_out = len_in;
    }
  }
  else
  {
    if (type == BUCKET_TYPE_V33_LIZARD)
      type = BUCKET_TYPE_V33;
    ptr_out = ptr_in;
    len_out = len_in;
  }
  bputl(fb_out, type);
  bputl(fb_out, len_out);
  if (type == BUCKET_TYPE_V33_LIZARD)
  {
    bputl(fb_out, len_in);
    bputl(fb_out, adler32(ptr_in, len_in));
  }
  if (ptr_out == bb_out.ptr || ptr_out == ptr_in)
    bwrite(fb_out, ptr_out, len_out);
  else
    bdirect_write_commit(fb_out, ptr_out + len_out);
  return type;
}

int
lizard_bbcopy_compress(struct fastbuf *fb_out, struct fastbuf *fb_in, uns len_in)
{
  byte *ptr_in;
  uns i = bdirect_read_prepare(fb_in, &ptr_in);
  if (i < len_in)
  {
    bb_grow(&bb_in, len_in);
    bread(fb_in, bb_in.ptr, len_in);
    ptr_in = bb_in.ptr;
  }
  else
    bdirect_read_commit(fb_in, ptr_in + len_in);
  return lizard_bwrite(fb_out, ptr_in, len_in);
}

static int
decompress(struct lizard_buffer *liz_buf, byte *ptr_in, byte **ptr_out)
{
  uns orig_len = GET_U32(ptr_in);
  uns orig_adler = GET_U32(ptr_in + 4);
  ptr_in += 8;
  *ptr_out = lizard_decompress_safe(ptr_in, liz_buf, orig_len);
  if (!*ptr_out)
    return -1;
  if (adler32(*ptr_out, orig_len) != orig_adler)
  {
    errno = EINVAL;
    return -1;
  }
  return orig_len;
}

int
lizard_memread(struct lizard_buffer *liz_buf, byte *ptr_in, byte **ptr_out, uns *type)
{
  *type = GET_U32(ptr_in);
  if (*type < BUCKET_TYPE_PLAIN || *type > BUCKET_TYPE_V33_LIZARD)
  {
    errno = EINVAL;
    return -1;
  }
  uns stored_len = GET_U32(ptr_in + 4);
  ptr_in += 8;
  if (*type == BUCKET_TYPE_V33_LIZARD)
    return decompress(liz_buf, ptr_in, ptr_out);
  else
  {
    *ptr_out = ptr_in;
    return stored_len;
  }
}

int
lizard_bread(struct lizard_buffer *liz_buf, struct fastbuf *fb_in, byte **ptr_out, uns *type)
{
  *type = bgetl(fb_in);
  if (*type < BUCKET_TYPE_PLAIN || *type > BUCKET_TYPE_V33_LIZARD)
  {
    if (*type == ~0U)			// EOF
      errno = EBADF;
    else
      errno = EINVAL;
    return -1;
  }
  uns stored_len = bgetl(fb_in);
  uns want_len = stored_len + (*type == BUCKET_TYPE_V33_LIZARD ? 8 : 0);
  byte *ptr_in;
  uns i = bdirect_read_prepare(fb_in, &ptr_in);
  if (i < want_len)
  {
    bb_grow(&bb_in, want_len);
    bread(fb_in, bb_in.ptr, want_len);
    ptr_in = bb_in.ptr;
  }
  else
    bdirect_read_commit(fb_in, ptr_in + want_len);
  if (*type == BUCKET_TYPE_V33_LIZARD)
    return decompress(liz_buf, ptr_in, ptr_out);
  else
  {
    *ptr_out = ptr_in;
    return stored_len;
  }
}
