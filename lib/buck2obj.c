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
#include <unistd.h>

#define	MAX_HEADER_SIZE	1024		// extra space for the header not counted in MaxObjSize
#define	RET_ERR(num)	({ errno = num; return NULL; })

struct buck2obj_buf
{
  uns max_len, raw_len;
  byte *raw;
  struct lizard_buffer *lizard;
  struct mempool *mp;
};

static void
buck2obj_alloc_internal(struct buck2obj_buf *buf, uns max_len)
{
  buf->max_len = max_len;
  if (!max_len)
  {
    buf->raw_len = 0;
    buf->raw = NULL;
    return;
  }
  buf->raw_len = max_len * LIZARD_MAX_MULTIPLY + LIZARD_MAX_ADD + MAX_HEADER_SIZE;
  buf->raw = xmalloc(buf->raw_len);
}

struct buck2obj_buf *
buck2obj_alloc(struct mempool *mp)
{
  struct buck2obj_buf *buf = xmalloc(sizeof(struct buck2obj_buf));
  buck2obj_alloc_internal(buf, 0);
  buf->lizard = lizard_alloc(0);
  buf->mp = mp;
  return buf;
}

void
buck2obj_free(struct buck2obj_buf *buf)
{
  lizard_free(buf->lizard);
  if (buf->raw)
    xfree(buf->raw);
  xfree(buf);
}

static void
buck2obj_realloc(struct buck2obj_buf *buf, uns max_len)
{
  if (max_len <= buf->max_len)
    return;
  if (max_len < 2*buf->max_len)		// to ensure amortized logarithmic complexity
    max_len = 2*buf->max_len;
  if (buf->raw)
    xfree(buf->raw);
  buck2obj_alloc_internal(buf, max_len);
}

static inline byte *
decode_attributes(byte *ptr, byte *end, struct odes *o, uns can_overwrite)
{
  if (can_overwrite >= 2)
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
  else if (can_overwrite == 1)
    while (ptr < end)
    {
      uns len;
      GET_UTF8(ptr, len);
      if (!len--)
	break;
      byte type = ptr[len];

      ptr[len] = 0;
      obj_add_attr(o, type, ptr);
      ptr[len] = type;

      ptr += len + 1;
    }
  else
    while (ptr < end)
    {
      uns len;
      GET_UTF8(ptr, len);
      if (!len--)
	break;
      byte type = ptr[len];

      byte *dup = mp_alloc_fast_noalign(o->pool, len+1);
      memcpy(dup, ptr, len);
      dup[len] = 0;
      obj_add_attr_ref(o, type, dup);

      ptr += len + 1;
    }
  return ptr;
}

struct odes *
obj_read_bucket(struct buck2obj_buf *buf, uns buck_type, struct fastbuf *body, uns want_body)
{
  mp_flush(buf->mp);
  struct odes *o = obj_new(buf->mp);

  if (buck_type < BUCKET_TYPE_V33)
  {
    if (want_body)			// ignore empty lines, read until EOF
      obj_read_multi(body, o);
    else				// end on EOF or the first empty line
      obj_read(body, o);
  }
  else
  {
    /* Compute the length of the bucket.  We cannot fetch this attribute
     * directly due to remote indexing.  */
    bseek(body, 0, SEEK_END);
    sh_off_t buck_len = btell(body);
    bsetpos(body, 0);

    /* Read all the bucket into 1 buffer, 0-copy if possible.  */
    int can_overwrite = bconfig(body, BCONFIG_CAN_OVERWRITE, 0);
    if (can_overwrite < 0)
      can_overwrite = 0;
    uns overwritten;
    byte *ptr, *end;
    uns len = bdirect_read_prepare(body, &ptr);
    if (len < buck_len
    || (can_overwrite < 2 && buck_type == BUCKET_TYPE_V33))
    {
      /* Copy if the original buffer is too small.
       * If it is write-protected, copy it also if it is uncompressed.  */
      if (buck_len > buf->raw_len)
	buck2obj_realloc(buf, buck_len);
      len = bread(body, buf->raw, buck_len);
      ptr = buf->raw;
      can_overwrite = 2;
      overwritten = 0;
    }
    else
      overwritten = can_overwrite > 1;
    end = ptr + len;

    ptr = decode_attributes(ptr, end, o, can_overwrite);// header
    if (!want_body)
      return o;
    if (buck_type == BUCKET_TYPE_V33)
      ;
    else if (buck_type == BUCKET_TYPE_V33_LIZARD)	// decompression
    {
      len = GET_U32(ptr);
      ptr += 4;
      int res;
decompress:
      res = lizard_decompress_safe(ptr, buf->lizard, len);
      if (res != (int) len)
      {
	if (res >= 0)
	  errno = EINVAL;
	else if (errno == EFBIG)
	{
	  lizard_realloc(buf->lizard, len);
	  goto decompress;
	}
	else
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
    /* If (overwritten), bflush(body) might be needed.  */
  }
  return o;
}
