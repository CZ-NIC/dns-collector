/*
 *	Bucket -> Object converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "charset/unicode.h"
#include "lib/object.h"
#include "lib/bucket.h"
#include "lib/lizard.h"
#include "lib/buck2obj.h"

#include <errno.h>

static inline byte *
decode_attributes(byte *start, byte *end, struct odes *o)
{
  byte *p = start;
  while (p < end)
  {
    uns len;
    GET_UTF8(p, len);
    if (!len)
      break;
    byte type = p[len];
    p[len] = 0;
    obj_add_attr(o, type, p);
  }
  return p;
}

int
extract_odes(struct obuck_header *hdr, struct fastbuf *body, struct odes *o, byte *buf, uns buf_len, struct lizard_buffer *lizard_buf)
{
  if (hdr->type < BUCKET_TYPE_V30C)
  {
    oa_allocate = 1;
    obj_read_multi(body, o);
  }
  else
  {
    oa_allocate = 0;
    /* Read all the bucket into 1 buffer, 0-copy if possible.  */
    byte *start, *end;
    uns len = bdirect_read_prepare(body, &start);
    if (len < hdr->length)
    {
      if (hdr->length > buf_len)
      {
	errno = EFBIG;
	return -1;
      }
      len = bread(body, buf, hdr->length);
      start = buf;
    }
    end = start + len;

    /* Decode the header, 0-copy.  */
    byte *p = decode_attributes(start, end, o);

    /* Decompress the body.  */
    if (hdr->type == BUCKET_TYPE_V30C)
    {
      GET_UTF8(p, len);
      int res = lizard_decompress_safe(p, lizard_buf, len);
      if (res < 0)
	return res;
      if (res != (int) len)
      {
	errno = EINVAL;
	return -1;
      }
      start = lizard_buf->ptr;
      end = start + len;
    }
    else
      start = p;

    /* Decode the body, 0-copy.  */
    p = decode_attributes(start, end, o);
    if (p != end)
    {
      errno = EINVAL;
      return -1;
    }
  }
  return 0;
}
