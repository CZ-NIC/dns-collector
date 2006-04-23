#ifndef _IMAGES_IMAGE_DUP_H
#define _IMAGES_IMAGE_DUP_H

struct image;

struct image_dup {
  struct image *image;
  byte *buf;
  uns flags;
  uns cols;
  uns rows;
  uns line;
  uns width;
  uns height;
};

#define IMAGE_DUP_FLAG_SCALE	0x1

#define IMAGE_DUP_TRANS_ID	0x01
#define IMAGE_DUP_TRANS_ALL	0xff

void image_dup_init(struct image_dup *dup, struct image *image, struct mempool *pool);
int image_dup_compare(struct image_dup *dup1, struct image_dup *dup2, uns trans);

#endif
