#ifndef _IMAGES_OBJECT_H
#define _IMAGES_OBJECT_H

#include <images/images.h>

struct image_obj_info {
  uns cols;
  uns rows;
  uns colors;
  enum image_format thumb_format;
  uns thumb_cols;
  uns thumb_rows;
  uns thumb_size;
  byte *thumb_data;
};

struct odes;
struct mempool;
struct image_signature;

uns get_image_obj_info(struct image_obj_info *ioi, struct odes *o);
uns get_image_obj_thumb(struct image_obj_info *ioi, struct odes *o, struct mempool *pool);
struct image *read_image_obj_thumb(struct image_obj_info *ioi, struct fastbuf *fb, struct image_io *io, struct mempool *pool);
void put_image_obj_signature(struct odes *o, struct image_signature *sig);
uns get_image_obj_signature(struct image_signature *sig, struct odes *o);

#endif
