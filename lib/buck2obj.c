/*
 *	Generating Objects from Buckets
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *	(c) 2004, Martin Mares <mj@ucw.cz>
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/unaligned.h"
#include "lib/mempool.h"
#include "lib/fastbuf.h"
#include "lib/unicode.h"
#include "lib/object.h"
#include "lib/bucket.h"
#include "lib/lizard.h"
#include "lib/bbuf.h"
#include "lib/ff-utf8.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define	RET_ERR(num)	({ errno = num; return -1; })

struct buck2obj_buf
{
  bb_t bb;
  struct lizard_buffer *lizard;
};

static uns get_attr_type;

void
get_attr_set_type(uns type)
{
  if (type < BUCKET_TYPE_PLAIN || type > BUCKET_TYPE_V33_LIZARD)
    die("Unknown buckettype %x", type);
  get_attr_type = type;
}

int
get_attr(byte **pos, byte *end, struct parsed_attr *attr)
{
  byte *ptr = *pos;
  if (ptr >= end)
    return -1;
  if (get_attr_type < BUCKET_TYPE_V33)
  {
    if (get_attr_type == BUCKET_TYPE_PLAIN)
    {
      while (ptr < end && *ptr == '\n')
	ptr++;
      *pos = ptr;
      if (ptr >= end)
	return -1;
    }
    else if (*ptr == '\n')
    {
      *pos = ++ptr;
      return 0;
    }
    attr->attr = *ptr++;
    attr->val = ptr;
    while (ptr < end && *ptr != '\n')
      ptr++;
    attr->len = ptr++ - attr->val;
  }
  else
  {
    uns len;
    GET_UTF8_32(ptr, len);
    if (!len--)
    {
      *pos = ptr;
      return 0;
    }
    attr->attr = ptr[len];
    attr->val = ptr;
    attr->len = len;
    ptr += len+1;
  }
  if (ptr > end)
    die("Incomplete attribute %c", attr->attr);
  *pos = ptr;
  return attr->attr;
}

int
bget_attr(struct fastbuf *b, struct parsed_attr *attr)
{
  static bb_t buf;
  if (get_attr_type < BUCKET_TYPE_V33)
  {
    int c = bgetc(b);
    if (c < 0)
      return -1;
    if (get_attr_type == BUCKET_TYPE_PLAIN)
    {
      while (c == '\n')
	c = bgetc(b);
      if (c < 0)
	return -1;
    }
    else if (c == '\n')
      return 0;
    attr->attr = c;

    byte *ptr, *end;
    uns len = bdirect_read_prepare(b, &ptr);
    end = ptr + len;
    attr->val = ptr;
    while (ptr < end && *ptr != '\n')
      ptr++;
    if (ptr < end)
    {
      bdirect_read_commit(b, ptr+1);
      attr->len = ptr - attr->val;
      return attr->attr;
    }

    len = 0;
    c = bgetc(b);
    while (c >= 0 && c != '\n')
    {
      bb_grow(&buf, len+1);
      buf.ptr[len++] = c;
      c = bgetc(b);
    }
    if (c < 0)
      die("Incomplete attribute %c", attr->attr);
    attr->val = buf.ptr;
    attr->len = len;
  }
  else
  {
    int len = bget_utf8_32(b);
    if (len <= 0)
      return len < 0 ? -1 : 0;
    attr->len = len-1;

    byte *ptr;
    int avail = bdirect_read_prepare(b, &ptr);
    if (avail >= len)
    {
      attr->val = ptr;
      attr->attr = ptr[len-1];
      bdirect_read_commit(b, ptr + len);
      return attr->attr;
    }
    bb_grow(&buf, --len);
    breadb(b, buf.ptr, len);
    attr->val = buf.ptr;
    attr->len = len;
    attr->attr = bgetc(b);
    if (attr->attr < 0)
      die("Incomplete attribute %c", attr->attr);
  }
  return attr->attr;
}

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
      GET_UTF8_32(ptr, len);
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
      GET_UTF8_32(ptr, len);
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

int
buck2obj_parse(struct buck2obj_buf *buf, uns buck_type, uns buck_len, struct fastbuf *body, struct odes *o_hdr, uns *body_start, struct odes *o_body)
{
  if (buck_type <= BUCKET_TYPE_PLAIN)
  {
    if (body_start)			// there is no header part
      *body_start = 0;
    // ignore empty lines and read until the end of the bucket
    sh_off_t end = btell(body) + buck_len;
    byte buf[MAX_ATTR_SIZE];
    while (btell(body) < end && bgets(body, buf, sizeof(buf)))
      if (buf[0])
	obj_add_attr(o_hdr, buf[0], buf+1);
    ASSERT(btell(body) == end);
  }
  else if (buck_type == BUCKET_TYPE_V30)
  {
    sh_off_t start = btell(body);
    sh_off_t end = start + buck_len;
    byte buf[MAX_ATTR_SIZE];
    while (btell(body) < end && bgets(body, buf, sizeof(buf)) && buf[0])
      obj_add_attr(o_hdr, buf[0], buf+1);
    if (body_start)
      *body_start = btell(body) - start;
    else
    {
      while (btell(body) < end && bgets(body, buf, sizeof(buf)))
	if (buf[0])
	  obj_add_attr(o_body, buf[0], buf+1);
      ASSERT(btell(body) == end);
    }
  }
  else if (buck_type == BUCKET_TYPE_V33 || buck_type == BUCKET_TYPE_V33_LIZARD)
  {
    /* Avoid reading the whole bucket if only its header is needed.  */
    if (body_start)
    {
      sh_off_t start = btell(body);
      sh_off_t end = start + buck_len;
      while (btell(body) < end)
      {
	uns len = bget_utf8_32(body);
	if (!len)
	  break;
	byte *buf = mp_alloc_fast_noalign(o_hdr->pool, len);
	bread(body, buf, len);
	uns type = buf[--len];
	buf[len] = 0;
	obj_add_attr_ref(o_hdr, type, buf);
      }
      *body_start = btell(body) - start;
      return 0;
    }

    /* Read all the bucket into 1 buffer, 0-copy if possible.  */
    byte *ptr, *end;
    uns len = bdirect_read_prepare(body, &ptr);
    uns copied = 0;
    if (len < buck_len
    || (body->can_overwrite_buffer < 2 && buck_type == BUCKET_TYPE_V33))
    {
      /* Copy if the original buffer is too small.
       * If it is write-protected, copy it also if it is uncompressed.  */
      DBG("NO ZC: %d < %d, %d %08x", len, buck_len, body->can_overwrite_buffer, buck_type);
      bb_grow(&buf->bb, buck_len);
      len = bread(body, buf->bb.ptr, buck_len);
      ptr = buf->bb.ptr;
      copied = 1;
    }
    else
      DBG("ZC (%d >= %d, %d %08x)", len, buck_len, body->can_overwrite_buffer, buck_type);
    end = ptr + buck_len;

    ptr = decode_attributes(ptr, end, o_hdr, 0);		// header
    if (buck_type == BUCKET_TYPE_V33_LIZARD)		// decompression
    {
      if (ptr + 8 > end)
	{
	  if (ptr == end)				// truncated bucket
	    goto commit;
	  RET_ERR(EINVAL);
	}
      len = GET_U32(ptr);
      ptr += 4;
      uns adler = GET_U32(ptr);
      ptr += 4;
      byte *new_ptr = lizard_decompress_safe(ptr, buf->lizard, len);
      if (!new_ptr)
	return -1;
      if (adler32(new_ptr, len) != adler)
	RET_ERR(EINVAL);
      if (!copied)
	bdirect_read_commit(body, end);
      ptr = new_ptr;
      end = ptr + len;
      copied = 1;
    }
    ptr = decode_attributes(ptr, end, o_body, 2);	// body
    if (ptr != end)
      RET_ERR(EINVAL);
  commit:
    if (!copied)
      bdirect_read_commit_modified(body, ptr);
  }
  else
    {
      bskip(body, buck_len);
      RET_ERR(EINVAL);
    }
  return 0;
}

struct odes *
obj_read_bucket(struct buck2obj_buf *buf, struct mempool *pool, uns buck_type, uns buck_len, struct fastbuf *body, uns *body_start)
{
  struct odes *o = obj_new(pool);
  if (buck2obj_parse(buf, buck_type, buck_len, body, o, body_start, o) < 0)
    return NULL;
  else
    return o;
}

int
obj_read(struct fastbuf *f, struct odes *o)
{
  byte buf[MAX_ATTR_SIZE];

  while (bgets(f, buf, sizeof(buf)))
    {
      if (!buf[0])
	return 1;
      obj_add_attr(o, buf[0], buf+1);
    }
  return 0;
}
