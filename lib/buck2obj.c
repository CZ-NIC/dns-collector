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
decode_attributes(byte *ptr, byte *end, struct odes *o)
{
  /* FIXME: this forbids storing attributes with empty string as a value.
   * Verify whether it is used or not.  */
  while (ptr < end)
  {
    uns len;
    GET_UTF8(ptr, len);
    if (!len)
      break;
    byte type = ptr[len];
    ptr[len] = 0;
    obj_add_attr(o, type, ptr);
    ptr += len + 1;
  }
  return ptr;
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
    byte *ptr, *end;
    uns len = bdirect_read_prepare(body, &ptr);		// WARNING: must NOT use mmaped-I/O
    if (len < hdr->length)
    {
      if (hdr->length > buf_len)
      {
	errno = EFBIG;
	return -1;
      }
      len = bread(body, buf, hdr->length);
      ptr = buf;
    }
    end = ptr + len;

    ptr = decode_attributes(ptr, end, o);		// header
    if (hdr->type == BUCKET_TYPE_V30C)			// decompression
    {
      GET_UTF8(ptr, len);
      int res = lizard_decompress_safe(ptr, lizard_buf, len);
      if (res != (int) len)
      {
	if (res < 0)
	  return res;
	errno = EINVAL;
	return -1;
      }
      ptr = lizard_buf->ptr;
      end = ptr + len;
    }
    ptr = decode_attributes(ptr, end, o);		// body

    if (ptr != end)
    {
      errno = EINVAL;
      return -1;
    }
  }
  return 0;
}
