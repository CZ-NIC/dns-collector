#ifndef _IMAGES_OBJECT_H
#define _IMAGES_OBJECT_H

#include <images/images.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define get_image_obj_info ucw_get_image_obj_info
#define get_image_obj_signature ucw_get_image_obj_signature
#define get_image_obj_thumb ucw_get_image_obj_thumb
#define put_image_obj_signature ucw_put_image_obj_signature
#define read_image_obj_thumb ucw_read_image_obj_thumb
#endif

struct image_obj_info {
  uint cols;
  uint rows;
  uint colors;
  enum image_format thumb_format;
  uint thumb_cols;
  uint thumb_rows;
  uint thumb_size;
  byte *thumb_data;
};

struct odes;
struct mempool;
struct image_signature;

uint get_image_obj_info(struct image_obj_info *ioi, struct odes *o);
uint get_image_obj_thumb(struct image_obj_info *ioi, struct odes *o, struct mempool *pool);
struct image *read_image_obj_thumb(struct image_obj_info *ioi, struct fastbuf *fb, struct image_io *io, struct mempool *pool);
void put_image_obj_signature(struct odes *o, struct image_signature *sig);
uint get_image_obj_signature(struct image_signature *sig, struct odes *o);

#endif
