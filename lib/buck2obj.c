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

#define	MAX_HEADER_SIZE	1024		// extra space for the header not counted in MaxObjSize
#define	RET_ERR(num)	({ errno = num; return NULL; })

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
decode_attributes(byte *ptr, byte *end, struct odes *o, uns can_overwrite)
{
  while (ptr < end)
  {
    uns len;
    GET_UTF8(ptr, len);
    if (!len--)
      break;
    byte type = ptr[len];
    if (can_overwrite == 2)
    {
      ptr[len] = 0;
      obj_add_attr_ref(o, type, ptr);
    }
    else if (can_overwrite == 1)
    {
      ptr[len] = 0;
      obj_add_attr(o, type, ptr);
      ptr[len] = type;
    }
    else
    {
      byte *dup = mp_alloc(o->pool, len+1);
      memcpy(dup, ptr, len);
      dup[len] = 0;
      obj_add_attr_ref(o, type, ptr);
    }
    ptr += len + 1;
  }
  return ptr;
}

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
    int can_overwrite = MAX(bconfig(body, BCONFIG_CAN_OVERWRITE, 0), 0);
    uns overwritten;
    byte *ptr, *end;
    uns len = bdirect_read_prepare(body, &ptr);
    if (len < hdr->length
    || (can_overwrite < 2 && hdr->type == BUCKET_TYPE_V33))
    {
      /* Copy if the original buffer is too small.
       * If it is write-protected, copy it also if it is uncompressed.  */
      if (hdr->length > buf->raw_len)
	RET_ERR(EFBIG);
      len = bread(body, buf->raw, hdr->length);
      ptr = buf->raw;
      can_overwrite = 2;
      overwritten = 0;
    }
    else
      overwritten = can_overwrite > 1;
    end = ptr + len;

    ptr = decode_attributes(ptr, end, o, can_overwrite);// header
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
      can_overwrite = 2;
    }
    else						// unknown bucket type
      RET_ERR(EINVAL);
    ASSERT(can_overwrite == 2);				// because of the policy and decompression
    ptr = decode_attributes(ptr, end, o, 2);		// body

    if (ptr != end)
      RET_ERR(EINVAL);
    if (overwritten)
      bflush(body);
  }
  return o;
}
