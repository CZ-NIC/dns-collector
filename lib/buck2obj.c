/*
 *	Bucket -> Object converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/unaligned.h"
#include "lib/pools.h"
#include "lib/fastbuf.h"
#include "charset/unicode.h"
#include "lib/object.h"
#include "lib/bucket.h"
#include "lib/lizard.h"
#include "lib/buck2obj.h"

#include <stdlib.h>
#include <errno.h>

#define	MAX_HEADER_SIZE	1024		// extra space for the header not countet to MaxObjSize

struct buck2obj_buf
{
  byte *raw;
  uns raw_len;
  struct lizard_buffer *lizard;
  struct mempool *mp;
};

struct buck2obj_buf *
buck2obj_alloc(uns max_len, struct mempool *mp)
{
  struct buck2obj_buf *buf = xmalloc(sizeof(struct buck2obj_buf));
  buf->raw_len = max_len * LIZARD_MAX_MULTIPLY + LIZARD_MAX_ADD + MAX_HEADER_SIZE;
  buf->raw = xmalloc(buf->raw_len);
  buf->lizard = lizard_alloc(max_len);
  buf->mp = mp;
  return buf;
}

void
buck2obj_free(struct buck2obj_buf *buf)
{
  lizard_free(buf->lizard);
  xfree(buf->raw);
  xfree(buf);
}

static inline byte *
decode_attributes(byte *ptr, byte *end, struct odes *o)
{
  while (ptr < end)
  {
    uns len;
    GET_UTF8(ptr, len);
    if (!len--)
      break;
    byte type = ptr[len];
    ptr[len] = 0;
    obj_add_attr_ref(o, type, ptr);
    ptr += len + 1;
  }
  return ptr;
}

#define	RET_ERR(num)	({ errno = num; return NULL; })
struct odes *
buck2obj_convert(struct buck2obj_buf *buf, struct obuck_header *hdr, struct fastbuf *body)
{
  mp_flush(buf->mp);
  struct odes *o = obj_new(buf->mp);

  if (hdr->type < BUCKET_TYPE_V33)
    obj_read_multi(body, o);
  else
  {
    /* Read all the bucket into 1 buffer, 0-copy if possible.  */
    byte *ptr, *end;
    uns len = bdirect_read_prepare(body, &ptr);		// WARNING: must NOT use mmaped-I/O
    if (len < hdr->length)
    {
      if (hdr->length > buf->raw_len)
	RET_ERR(EFBIG);
      len = bread(body, buf->raw, hdr->length);
      ptr = buf->raw;
    }
    end = ptr + len;

    ptr = decode_attributes(ptr, end, o);		// header
    if (hdr->type == BUCKET_TYPE_V33)
      ;
    else if (hdr->type == BUCKET_TYPE_V33_LIZARD)	// decompression
    {
      len = GET_U32(ptr);
      ptr += 4;
      int res = lizard_decompress_safe(ptr, buf->lizard, len);
      if (res != (int) len)
      {
	if (res >= 0)
	  errno = EINVAL;
	return NULL;
      }
      ptr = buf->lizard->ptr;
      end = ptr + len;
    }
    else						// unknown bucket type
      RET_ERR(EINVAL);
    ptr = decode_attributes(ptr, end, o);		// body

    if (ptr != end)
      RET_ERR(EINVAL);
  }
  return o;
}
