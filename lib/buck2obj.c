/*
 *	Bucket -> Object Converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *	(c) 2004, Martin Mares <mj@ucw.cz>
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
};

struct buck2obj_buf *
buck2obj_alloc(void)
{
  struct buck2obj_buf *buf = xmalloc(sizeof(struct buck2obj_buf));
  bb_init(&buf->bb);
  buf->lizard = lizard_alloc();
  return buf;
}

void
buck2obj_free(struct buck2obj_buf *buf)
{
  lizard_free(buf->lizard);
  bb_done(&buf->bb);
  xfree(buf);
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
obj_read_bucket(struct buck2obj_buf *buf, struct mempool *pool, uns buck_type, uns buck_len, struct fastbuf *body, uns *body_start)
{
  struct odes *o = obj_new(pool);

  if (buck_type == BUCKET_TYPE_PLAIN || buck_type == BUCKET_TYPE_V30)
  {
    if (!body_start)			// header + body: ignore empty lines, read until EOF or NUL
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
  else if (buck_type == BUCKET_TYPE_V33 || buck_type == BUCKET_TYPE_V33_LIZARD)
  {
    /* Read all the bucket into 1 buffer, 0-copy if possible.  */
    byte *ptr, *end;
    uns len = bdirect_read_prepare(body, &ptr);
    uns copied = 0;
    if (len < buck_len
    || (body->can_overwrite_buffer < 2 && buck_type == BUCKET_TYPE_V33))
    {
      /* Copy if the original buffer is too small.
       * If it is write-protected, copy it also if it is uncompressed.  */
      bb_grow(&buf->bb, buck_len);
      len = bread(body, buf->bb.ptr, buck_len);
      ptr = buf->bb.ptr;
      copied = 1;
    }
    end = ptr + len;

    byte *start = ptr;
    ptr = decode_attributes(ptr, end, o, 0);		// header
    if (body_start)
    {
      *body_start = ptr - start;
      if (!copied)
	bdirect_read_commit(body, ptr);
      return o;
    }
    if (buck_type == BUCKET_TYPE_V33_LIZARD)		// decompression
    {
      /* FIXME: Add checks for len<4 and other format violations */
      len = GET_U32(ptr);
      ptr += 4;
      byte *new_ptr = lizard_decompress_safe(ptr, buf->lizard, len);
      if (!new_ptr)
	return NULL;
      if (!copied)
	bdirect_read_commit(body, end);
      ptr = new_ptr;
      end = ptr + len;
      copied = 1;
    }
    ptr = decode_attributes(ptr, end, o, 2);		// body
    if (ptr != end)
      RET_ERR(EINVAL);
    if (!copied)
      bdirect_read_commit_modified(body, ptr);
  }
  else
    RET_ERR(EINVAL);
  return o;
}
