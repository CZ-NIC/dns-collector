#ifndef _IMAGES_DUP_CMP_H
#define _IMAGES_DUP_CMP_H

struct image_dup {
  struct image *image;
  byte *tab_pixels;
  u32 tab_cols;
  u32 tab_rows;
  u32 tab_row_size;
  u32 tab_size;
};

#define IMAGE_DUP_TRANS_ID	0x01
#define IMAGE_DUP_FLIP_X	0x02
#define IMAGE_DUP_FLIP_Y	0x04
#define IMAGE_DUP_ROT_180	0x08
#define IMAGE_DUP_FLIP_BACK	0x10
#define IMAGE_DUP_ROT_CCW	0x20
#define IMAGE_DUP_ROT_CW	0x40
#define IMAGE_DUP_FLIP_SLASH	0x80
#define IMAGE_DUP_TRANS_ALL	0xff
#define IMAGE_DUP_SCALE		0x100
#define IMAGE_DUP_WANT_ALL	0x200

int image_dup_init(struct image_thread *thread, struct image_dup *dup, struct image *image, struct mempool *pool);
int image_dup_compare(struct image_dup *dup1, struct image_dup *dup2, uns flags);

#endif
