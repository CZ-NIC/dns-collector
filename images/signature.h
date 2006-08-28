#ifndef _IMAGES_SIGNATURE_H
#define _IMAGES_SIGNATURE_H

/* Configuration */
extern uns image_sig_min_width, image_sig_min_height;
extern uns *image_sig_prequant_thresholds;
extern uns image_sig_postquant_min_steps, image_sig_postquant_max_steps, image_sig_postquant_threshold;
extern double image_sig_textured_threshold;
extern uns image_sig_compare_method;

#define IMAGE_VEC_F	6
#define IMAGE_REG_F	IMAGE_VEC_F
#define IMAGE_REG_H	3
#define IMAGE_REG_MAX	16

/* K-dimensional feature vector (6 bytes) */
struct image_vector {
  byte f[IMAGE_VEC_F];		/* texture features */
} PACKED;

/* Fetures for image regions (16 bytes) */
struct image_region {
  byte f[IMAGE_VEC_F];		/* texture features */
  u16 h[IMAGE_REG_H];		/* shape features */
  u16 wa;			/* normalized area percentage */
  u16 wb;			/* normalized weight */
} PACKED;

#define IMAGE_SIG_TEXTURED	0x1

/* Image signature (11 + len * 16 bytes) */
struct image_signature {
  byte len;			/* Number of regions */
  byte flags;			/* IMAGE_SIG_xxx */
  byte df;			/* Average f dist */
  u16 dh;			/* Average h dist */
  struct image_vector vec;	/* Combination of all regions... simple signature */
  struct image_region reg[IMAGE_REG_MAX];/* Feature vector for every region */
} PACKED;

static inline uns
image_signature_size(uns len)
{
  return 5 + sizeof(struct image_vector) + len * sizeof(struct image_region);
}

/* sig-dump.c */

#define IMAGE_VECTOR_DUMP_MAX (IMAGE_VEC_F * 16 + 1)
#define IMAGE_REGION_DUMP_MAX ((IMAGE_REG_F + IMAGE_REG_H) * 16 + 100)

byte *image_vector_dump(byte *buf, struct image_vector *vec);
byte *image_region_dump(byte *buf, struct image_region *reg);

struct image_sig_block {
  struct image_sig_block *next;		/* linked list */
  u32 x, y;				/* block position */
  byte area;				/* block area in pixels (usually 16) */
  byte region;				/* region index */
  byte v[IMAGE_VEC_F];			/* feature vector */
};

struct image_sig_region {
  struct image_sig_block *blocks;
  u32 count;
  u32 a[IMAGE_VEC_F];
  u32 b[IMAGE_VEC_F];
  u32 c[IMAGE_VEC_F];
  u64 e;
  u64 w_sum;
};

struct image_sig_data {
  struct image *image;
  struct image_sig_block *blocks;
  struct image_sig_region regions[IMAGE_REG_MAX];
  u32 cols;
  u32 rows;
  u32 full_cols;
  u32 full_rows;
  u32 flags;
  u32 area;
  u32 valid;
  u32 blocks_count;
  u32 regions_count;
  u32 f[IMAGE_VEC_F];
};

/* sig-init.c */

int compute_image_signature(struct image_thread *thread, struct image_signature *sig, struct image *image);

int image_sig_init(struct image_thread *thread, struct image_sig_data *data, struct image *image);
void image_sig_preprocess(struct image_sig_data *data);
void image_sig_finish(struct image_sig_data *data, struct image_signature *sig);
void image_sig_cleanup(struct image_sig_data *data);

/* sig-seg.c */

void image_sig_segmentation(struct image_sig_data *data);

/* sig-txt.c */

void image_sig_detect_textured(struct image_sig_data *data);

/* sig-cmp.c */

#define IMAGE_SIG_DIST_SCALE 30

uns image_signatures_dist(struct image_signature *sig1, struct image_signature *sig2);

#endif

