#ifndef _IMAGES_SIGNATURE_H
#define _IMAGES_SIGNATURE_H

#define IMAGE_VEC_F	6
#define IMAGE_REG_F	IMAGE_VEC_F
#define IMAGE_REG_H	3
#define IMAGE_REG_MAX	8

/* K-dimensional feature vector */
struct image_vector {
  byte f[IMAGE_VEC_F];		/* texture features */
} PACKED;

/* Fetures for image regions */
struct image_region {
  byte f[IMAGE_VEC_F];		/* texture features */
  u16 h[IMAGE_REG_H];		/* shape features */
  byte wa;			/* normalized area percentage */
  byte wb;			/* normalized weight */
} PACKED;

/* Image signature */
struct image_signature {
  struct image_vector vec;	/* Combination of all regions... simple signature */
  byte len;			/* Number of regions */
  byte df;			/* average f dist */
  u16 dh;			/* average h dist */
  struct image_region reg[IMAGE_REG_MAX];/* Feature vector for every region */
};

/* sig-dump.c */

#define IMAGE_VECTOR_DUMP_MAX (IMAGE_VEC_F * 16 + 1)
#define IMAGE_REGION_DUMP_MAX ((IMAGE_REG_F + IMAGE_REG_H) * 16 + 100)

byte *image_vector_dump(byte *buf, struct image_vector *vec);
byte *image_region_dump(byte *buf, struct image_region *reg);

/* sig-init.c */

void bget_image_signature(struct fastbuf *fb, struct image_signature *sig);
void bput_image_signature(struct fastbuf *fb, struct image_signature *sig);
int compute_image_signature(struct image_thread *thread, struct image_signature *sig, struct image *image);

/* sig-cmp.c */

#define IMAGE_SIG_DIST_SCALE (3 + 3 + 8 + 16)

uns image_signatures_dist(struct image_signature *sig1, struct image_signature *sig2);

#if 0
/* K-dimensional interval */
struct image_bbox {
  struct image_vector vec[2];
};

/* Similarity search tree... will be changed */
struct image_tree {
  uns count;			/* Number of images in the tree */
  uns depth;			/* Tree depth */
  struct image_bbox bbox;	/* Bounding box containing all the */
  struct image_node *nodes;	/* Internal nodes */
  struct image_leaf *leaves;	/* Leaves */
};

/* Internal node in the search tree */
#define IMAGE_NODE_LEAF		0x80000000		/* Node contains pointer to leaves array */
#define IMAGE_NODE_DIM		0xff			/* Split dimension */
struct image_node {
  u32 val;
};

/* Leaves in the search tree */
#define IMAGE_LEAF_LAST		0x80000000		/* Last entry in the list */
#define IMAGE_LEAF_BITS(i)	(31 / IMAGE_VEC_K)	/* Number of bits for relative position in i-th dimension */
struct image_leaf {
  u32 flags;		/* Relative position in bbox and last node flag */ 
  oid_t oid;
};

#define stk_print_image_vector(v) ({ struct image_vector *_v = v; \
    byte *_s = (byte *) alloca(IMAGE_VEC_K * 6), *_p = _s + sprintf(_s, "%d", _v->f[0]); \
    for (uns _i = 1; _i < IMAGE_VEC_K; _i++) _p += sprintf(_p, " %d", _v->f[_i]); _s; })
#endif

#endif

