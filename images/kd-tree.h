#ifndef _IMAGES_KD_TREE_H
#define _IMAGES_KD_TREE_H

#define IMAGE_SEARCH_DIST_UNLIMITED	(~0U)

/* FIXME: support full length of oid_t, currently must be <2^31 */
#define IMAGE_SEARCH_ITEM_TYPE		0x80000000U
struct image_search_item {
  u32 dist;
  u32 index;
  struct image_bbox bbox;
};

struct image_search {
  struct image_tree *tree;
  struct image_node *nodes;
  struct image_leaf *leaves;
  struct image_vector query;
  struct image_search_item *buf;
  u32 *heap;
  uns count, visited, size, max_dist;
};

void image_search_init(struct image_search *is, struct image_tree *tree, struct image_vector *query, uns max_dist);
void image_search_done(struct image_search *is);
int image_search_next(struct image_search *is, oid_t *oid, uns *dist);

#endif
