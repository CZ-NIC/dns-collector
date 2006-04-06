#ifndef _IMAGES_H
#define _IMAGES_H

#include <stdio.h>
#include <magick/api.h>

#define IMAGE_VEC_K		6
#define IMAGE_VEC_LOG_K		3

typedef u32 image_vector[IMAGE_VEC_K];
typedef u32 image_box[2][IMAGE_VEC_K];

struct image_signature {
  image_vector vec; 
};

struct image_tree {
  uns count;
  uns depth;
  image_box box;
  struct image_node *nodes;
  struct image_entry *entries;
};

#define IMAGE_NODE_LEAF		0x80000000
#define IMAGE_NODE_DIM		((1 << IMAGE_VEC_LOG_K) - 1)

struct image_node {
  u32 value;
};

#define IMAGE_ENTRY_LAST	(1 << (sizeof(oid_t) * 8 - 1))

struct image_entry {
  oid_t oid;
};

int compute_image_signature(void *data, uns len, struct image_signature *sig);

#endif
