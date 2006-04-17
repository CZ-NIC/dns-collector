#ifndef _IMAGES_IMAGE_SIG_H
#define _IMAGES_IMAGE_SIG_H

#include "images/images.h"

#define IMAGE_VEC_K	6
#define IMAGE_REG_K	9
#define IMAGE_REG_MAX	4

typedef u16 image_feature_t;	/* 8 or 16 bits precision */

/* K-dimensional feature vector */
struct image_vector {
  image_feature_t f[IMAGE_VEC_K];
};

/* K-dimensional interval */
struct image_bbox {
  struct image_vector vec[2];
};

/* Fetures for image regions */
struct image_region {
  image_feature_t f[IMAGE_REG_K];
};

/* Image signature */
struct image_signature {
  struct image_vector vec;	/* Combination of all regions... simplier signature */
  image_feature_t len;		/* Number of regions */
  struct image_region reg[IMAGE_REG_MAX];/* Feature vector for every region */
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

int compute_image_signature(struct image *image, struct image_signature *sig);

#endif

