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

#define	RET_ERR(num)	({ errno = num; return NULL; })

#define	GBUF_TYPE	byte
#define	GBUF_PREFIX(x)	bb_##x
#include "lib/gbuf.h"

struct buck2obj_buf
{
  bb_t bb;
  struct lizard_buffer *lizard;
  struct mempool *mp;
};

struct buck2obj_buf *
buck2obj_alloc(struct mempool *mp)
{
  struct buck2obj_buf *buf = xmalloc(sizeof(struct buck2obj_buf));
  bb_init(&buf->bb);
  buf->lizard = lizard_alloc();
  buf->mp = mp;
  return buf;
}

void
buck2obj_free(struct buck2obj_buf *buf)
{
  lizard_free(buf->lizard);
  bb_done(&buf->bb);
  xfree(buf);
}

void
buck2obj_flush(struct buck2obj_buf *buf)
{
  mp_flush(buf->mp);
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
obj_read_bucket(struct buck2obj_buf *buf, uns buck_type, uns buck_len, struct fastbuf *body, uns *body_start)
{
  struct odes *o = obj_new(buf->mp);

  if (buck_type < BUCKET_TYPE_V33)
  {
    if (!body_start)			// header + body: ignore empty lines, read until EOF
    {
      obj_read_multi(body, o);
      bgetc(body);
    }
    else				// header only: end on EOF or the first empty line
    {
      sh_off_t start = btell(body);
      obj_read(body, o);
      *body_start = btell(body) - start;
    }
  }
  else
  {
    /* Read all the bucket into 1 buffer, 0-copy if possible.  */
    int can_overwrite = bconfig(body, BCONFIG_CAN_OVERWRITE, -1);
    /* FIXME: This could be cached in buck2obj_buf */
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
      bb_grow(&buf->bb, buck_len);
      len = bread(body, buf->bb.ptr, buck_len);
      ptr = buf->bb.ptr;
      can_overwrite = 2;
      overwritten = 0;
    }
    else
      overwritten = can_overwrite > 1;
    end = ptr + len;

    byte *start = ptr;
    ptr = decode_attributes(ptr, end, o, can_overwrite);// header
    if (body_start)
    {
      *body_start = ptr - start;
      return o;
    }
    if (buck_type == BUCKET_TYPE_V33)
      ;
    else if (buck_type == BUCKET_TYPE_V33_LIZARD)	// decompression
    {
      len = GET_U32(ptr);
      ptr += 4;
      int res;
      byte *new_ptr;
      res = lizard_decompress_safe(ptr, buf->lizard, len, &new_ptr);
      if (res != (int) len)
      {
	if (res >= 0)
	  errno = EINVAL;
	return NULL;
      }
      ptr = new_ptr;
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

byte *
obj_attr_to_bucket(byte *buf, uns buck_type, uns attr, byte *val)
{
  uns l;

  switch (buck_type)
    {
    case BUCKET_TYPE_PLAIN:
    case BUCKET_TYPE_V30:
      buf += sprintf(buf, "%c%s\n", attr, val);
      break;
    case BUCKET_TYPE_V33:
    case BUCKET_TYPE_V33_LIZARD:
      l = strlen(val) + 1;
      PUT_UTF8(buf, l);
      l--;
      memcpy(buf, val, l);
      buf += l;
      *buf++ = attr;
      break;
    default:
      die("obj_attr_to_bucket called for unknown type %08x", buck_type);
    }
  return buf;
}

byte *
obj_attr_to_bucket_num(byte *buf, uns buck_type, uns attr, uns val)
{
  byte vbuf[16];
  sprintf(vbuf, "%d", val);
  return obj_attr_to_bucket(buf, buck_type, attr, vbuf);
}
