/*
 *	Generating Buckets from Objects
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/ff-utf8.h"
#include "lib/bucket.h"
#include "lib/object.h"

#include <string.h>
#include <stdarg.h>

static uns use_v33;
static int hdr_sep;

void
attr_set_type(uns type)
{
  switch (type)
    {
    case BUCKET_TYPE_PLAIN:
      use_v33 = 0;
      hdr_sep = -1;
      break;
    case BUCKET_TYPE_V30:
      use_v33 = 0;
      hdr_sep = '\n';
      break;
    case BUCKET_TYPE_V33:
    case BUCKET_TYPE_V33_LIZARD:
      use_v33 = 1;
      hdr_sep = 0;
      break;
    default:
      die("Don't know how to generate buckets of type %08x", type);
    }
}

uns
size_attr(uns len)
{
  ASSERT(len <= MAX_ATTR_SIZE);
  if (use_v33)
    {
      len++;
      return len + utf8_space(len);
    }
  else
    return len + 2;
}

inline byte *
put_attr(byte *ptr, uns type, byte *val, uns len)
{
  if (use_v33)
  {
    PUT_UTF8(ptr, len+1);
    memcpy(ptr, val, len);
    ptr += len;
    *ptr++ = type;
  }
  else
  {
    *ptr++ = type;
    memcpy(ptr, val, len);
    ptr += len;
    *ptr++ = '\n';
  }
  return ptr;
}

byte *
put_attr_str(byte *ptr, uns type, byte *val)
{
  return put_attr(ptr, type, val, strlen(val));
}

inline byte *
put_attr_vformat(byte *ptr, uns type, byte *mask, va_list va)
{
  if (use_v33)
  {
    uns len = vsprintf(ptr+1, mask, va);
    if (len >= 127)
    {
      byte tmp[6], *tmp_end = tmp;
      PUT_UTF8(tmp_end, len+1);
      uns l = tmp_end - tmp;
      memmove(ptr+l, ptr+1, len);
      memcpy(ptr, tmp, l);
      ptr += l + len;
    }
    else
    {
      *ptr = len+1;
      ptr += len+1;
    }
    *ptr++ = type;
  }
  else
  {
    *ptr++ = type;
    ptr += vsprintf(ptr, mask, va);
    *ptr++ = '\n';
  }
  return ptr;
}

byte *
put_attr_format(byte *ptr, uns type, char *mask, ...)
{
  va_list va;
  va_start(va, mask);
  byte *ret = put_attr_vformat(ptr, type, mask, va);
  va_end(va);
  return ret;
}

byte *
put_attr_num(byte *ptr, uns type, uns val)
{
  if (use_v33)
  {
    uns len = sprintf(ptr+1, "%d", val) + 1;
    *ptr = len;
    ptr += len;
    *ptr++ = type;
  }
  else
    ptr += sprintf(ptr, "%c%d\n", type, val);
  return ptr;
}

byte *
put_attr_separator(byte *ptr)
{
  if (hdr_sep >= 0)
    *ptr++ = hdr_sep;
  return ptr;
}

inline void
bput_attr(struct fastbuf *b, uns type, byte *val, uns len)
{
  if (use_v33)
  {
    bput_utf8(b, len+1);
    bwrite(b, val, len);
    bputc(b, type);
  }
  else
  {
    bputc(b, type);
    bwrite(b, val, len);
    bputc(b, '\n');
  }
}

void
bput_attr_str(struct fastbuf *b, uns type, byte *val)
{
  bput_attr(b, type, val, strlen(val));
}

inline void
bput_attr_vformat(struct fastbuf *b, uns type, byte *mask, va_list va)
{
  if (use_v33)
  {
    int len = vsnprintf(NULL, 0, mask, va);
    if (len < 0)
      die("vsnprintf() does not support size=0");
    bput_utf8(b, len+1);
    vbprintf(b, mask, va);
    bputc(b, type);
  }
  else
  {
    bputc(b, type);
    vbprintf(b, mask, va);
    bputc(b, '\n');
  }
}

void
bput_attr_format(struct fastbuf *b, uns type, char *mask, ...)
{
  va_list va;
  va_start(va, mask);
  bput_attr_vformat(b, type, mask, va);
  va_end(va);
}

void
bput_attr_num(struct fastbuf *b, uns type, uns val)
{
  if (use_v33)
  {
    byte tmp[12];
    uns len = sprintf(tmp, "%d", val);
    bputc(b, len+1);
    bwrite(b, tmp, len);
    bputc(b, type);
  }
  else
    bprintf(b, "%c%d\n", type, val);
}

void
bput_attr_separator(struct fastbuf *b)
{
  if (hdr_sep >= 0)
    bputc(b, hdr_sep);
}

void
obj_write(struct fastbuf *f, struct odes *d)
{
  for(struct oattr *a=d->attrs; a; a=a->next)
    for(struct oattr *b=a; b; b=b->same)
      {
	byte *z;
	for (z = b->val; *z; z++)
	  if (*z < ' ' && *z != '\t')
	    {
	      log(L_ERROR, "obj_dump: Found non-ASCII character %02x (URL might be %s) in %c%s", *z, obj_find_aval(d, 'U'), a->attr, b->val);
	      *z = '?';
	    }
	ASSERT(z - b->val <= MAX_ATTR_SIZE-2);
	bput_attr_str(f, a->attr, b->val);
      }
}

void
obj_write_nocheck(struct fastbuf *f, struct odes *d)
{
  for(struct oattr *a=d->attrs; a; a=a->next)
    for(struct oattr *b=a; b; b=b->same)
      bput_attr_str(f, a->attr, b->val);
}
