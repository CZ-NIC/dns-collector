/*
 *	Image Library -- Image cards manipulations
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "ucw/base224.h"
#include "ucw/mempool.h"
#include "ucw/fastbuf.h"
#include "sherlock/object.h"
#include "images/images.h"
#include "images/object.h"
#include "images/color.h"
#include "images/signature.h"
#include <stdio.h>
#include <string.h>

uns
get_image_obj_info(struct image_obj_info *ioi, struct odes *o)
{
  byte *v = obj_find_aval(o, 'G');
  if (!v)
    {
      DBG("Missing image info attribute");
      return 0;
    }
  byte color_space[16], thumb_format[16];
  UNUSED uns cnt = sscanf(v, "%d%d%s%d%d%d%s", &ioi->cols, &ioi->rows, color_space,
      &ioi->colors, &ioi->thumb_cols, &ioi->thumb_rows, thumb_format);
  ASSERT(cnt == 7);
  ioi->thumb_format = (*thumb_format == 'p') ? IMAGE_FORMAT_PNG : IMAGE_FORMAT_JPEG;
  DBG("Readed image info attribute: dim=%ux%u", ioi->cols, ioi->rows);
  return 1;
}

uns
get_image_obj_thumb(struct image_obj_info *ioi, struct odes *o, struct mempool *pool)
{
  struct oattr *a = obj_find_attr(o, 'N');
  if (!a)
    {
      DBG("Missing image thumbnail attribute");
      return 0;
    }
  uns count = 0;
  uns max_len = 0;
  for (struct oattr *b = a; b; b = b->same)
    {
      count++;
      max_len += strlen(b->val);
    }
  byte buf[max_len + 1], *b = buf;
  for (; a; a = a->same)
    b += base224_decode(b, a->val, strlen(a->val));
  ASSERT(b != buf);
  ioi->thumb_data = mp_alloc(pool, ioi->thumb_size = b - buf);
  memcpy(ioi->thumb_data, buf, ioi->thumb_size);
  DBG("Readed thumbnail of size %u", ioi->thumb_size);
  return 1;
}

struct image *
read_image_obj_thumb(struct image_obj_info *ioi, struct fastbuf *fb, struct image_io *io, struct mempool *pool)
{
  struct fastbuf tmp_fb;
  if (!fb)
    fbbuf_init_read(fb = &tmp_fb, ioi->thumb_data, ioi->thumb_size, 0);
  io->format = ioi->thumb_format;
  io->fastbuf = fb;
  if (!image_io_read_header(io))
    goto error;
  io->pool = pool;
  io->flags = COLOR_SPACE_RGB | IMAGE_IO_USE_BACKGROUND;
  if (!io->background_color.color_space)
    io->background_color = color_white;
  struct image *img;
  if (!(img = image_io_read_data(io, 1)))
    goto error;
  DBG("Decompressed thumbnail: size=%ux%u", img->cols, img->rows);
  return img;
error:
  DBG("Failed to decompress thumbnail: %s", io->thread->err_msg);
  return NULL;
}

void
put_image_obj_signature(struct odes *o, struct image_signature *sig)
{
  /* signatures should be short enough to in a single attribute */
  uns size = image_signature_size(sig->len);
  byte buf[BASE224_ENC_LENGTH(size) + 1];
  buf[base224_encode(buf, (byte *)sig, size)] = 0;
  obj_set_attr(o, 'H', buf);
}

uns
get_image_obj_signature(struct image_signature *sig, struct odes *o)
{
  byte *a = obj_find_aval(o, 'H');
  if (!a)
    return 0;
  UNUSED uns size = base224_decode((byte *)sig, a, strlen(a));
  ASSERT(size == image_signature_size(sig->len));
  return 1;
}
