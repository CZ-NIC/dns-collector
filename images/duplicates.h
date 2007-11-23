#ifndef _IMAGES_DUPLICATES_H
#define _IMAGES_DUPLICATES_H

enum image_dup_flags {
  IMAGE_DUP_TRANS_ID =		0x0001,
  IMAGE_DUP_FLIP_X =		0x0002,
  IMAGE_DUP_FLIP_Y =		0x0004,
  IMAGE_DUP_ROT_180 =		0x0008,
  IMAGE_DUP_FLIP_BACK =		0x0010,
  IMAGE_DUP_ROT_CCW =		0x0020,
  IMAGE_DUP_ROT_CW =		0x0040,
  IMAGE_DUP_FLIP_SLASH =	0x0080,
  IMAGE_DUP_TRANS_ALL =		0x00ff,
  IMAGE_DUP_SCALE =		0x0100,
  IMAGE_DUP_WANT_ALL =		0x0200,
};

struct image_dup_context {
  struct image_context *ic;
  uns flags;
  uns ratio_threshold;
  uns error_threshold;
  uns qtree_limit;
  u64 sum_depth;
  u64 sum_pixels;
  uns error;
};

struct image_dup {
  struct image image;
  byte *tab_pixels;
  u32 tab_cols;
  u32 tab_rows;
  u32 tab_row_size;
  u32 tab_size;
};

/* dup-init.c */

void image_dup_context_init(struct image_context *ic, struct image_dup_context *ctx);
void image_dup_context_cleanup(struct image_dup_context *ctx);

uns image_dup_estimate_size(uns cols, uns rows, uns same_size_compare, uns qtree_limit);
uns image_dup_new(struct image_dup_context *ctx, struct image *image, void *buffer, uns same_size_compare);

/* dup-cmp.c */

uns image_dup_compare(struct image_dup_context *ctx, struct image_dup *dup1, struct image_dup *dup2);

/* internals */

static inline byte *
image_dup_block(struct image_dup *dup, uns tab_col, uns tab_row)
{
  return dup->tab_pixels + (dup->tab_row_size << tab_row) + (3 << (tab_row + tab_col));
}


#endif
