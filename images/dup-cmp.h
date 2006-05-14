#ifndef _IMAGES_DUP_CMP_H
#define _IMAGES_DUP_CMP_H

struct image_data;

struct image_dup {
  struct image_data *image;
  byte *buf;
  uns buf_size;
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

void image_dup_init(struct image_dup *dup, struct image_data *image, struct mempool *pool);
int image_dup_compare(struct image_dup *dup1, struct image_dup *dup2, uns trans);
uns image_dup_estimate_size(uns width, uns height);

#endif
