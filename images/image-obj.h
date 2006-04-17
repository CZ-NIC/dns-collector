/*
 *	Image Library -- Image Cards Manipulations
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _IMAGES_IMAGE_OBJ_H
#define _IMAGES_IMAGE_OBJ_H

#include "images/images.h"

struct mempool;
struct odes;

enum image_obj_flag {
  IMAGE_OBJ_INFO = 0x1,
  IMAGE_OBJ_THUMB_JPEG = 0x2,
  IMAGE_OBJ_THUMB_DATA = 0x4,
  IMAGE_OBJ_THUMB_IMAGE = 0x8
};

struct image_obj {
  struct odes *obj;
  struct mempool *pool;
  uns flags;
  uns width;
  uns height;
  byte *thumb_data;
  uns thumb_size;
  struct image thumb;
};

static inline void
imo_init(struct image_obj *imo, struct mempool *pool, struct odes *obj)
{
  imo->obj = obj;
  imo->pool = pool;
  imo->flags = 0;
}

void imo_decompress_thumbnails_init(void);
void imo_decompress_thumbnails_done(void);
int imo_decompress_thumbnail(struct image_obj *imo);

#endif
