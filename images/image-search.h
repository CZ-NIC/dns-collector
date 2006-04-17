#include "lib/heap.h"
#include <alloca.h>

#define IMAGE_SEARCH_DIST_UNLIMITED	(~0U)

/* FIXME: support full length of oid_t, currently must be <2^31 */
#define IMAGE_SEARCH_ITEM_TYPE		0x80000000U
struct image_search_item {
  u32 dist;
  u32 index;
  struct image_bbox bbox;
};

#define IMAGE_SEARCH_CMP(x,y) (is->buf[x].dist < is->buf[y].dist)

struct image_search {
  struct image_tree *tree;
  struct image_node *nodes;
  struct image_leaf *leaves;
  struct image_vector query;
  struct image_search_item *buf;
  u32 *heap;
  uns count, visited, size, max_dist;
};

#define SQR(x) ((x)*(x))

static void
image_search_init(struct image_search *is, struct image_tree *tree, struct image_vector *query, uns max_dist)
{
  // FIXME: empty tree
  is->tree = tree;
  is->nodes = tree->nodes;
  is->leaves = tree->leaves;
  is->query = *query;
  is->max_dist = max_dist;
  is->size = 0x1000;
  is->buf = xmalloc((is->size + 1) * sizeof(struct image_search_item));
  is->heap = xmalloc((is->size + 1) * sizeof(u32));
  is->visited = is->count = 1;
  is->heap[1] = 1;
  struct image_search_item *item = is->buf + 1;
  item->index = 1;
  item->bbox = tree->bbox;
  item->dist = 0;
  for (uns i = 0; i < IMAGE_VEC_K; i++)
    {
      if (query->f[i] < item->bbox.vec[0].f[i])
	item->dist += SQR(item->bbox.vec[0].f[i] - query->f[i]);
      else if (query->f[i] > item->bbox.vec[1].f[i])
	item->dist += SQR(query->f[i] - item->bbox.vec[0].f[i]);
      else
        {
	  item->dist = 0;
	  break;
	}
    }
}

static void
image_search_done(struct image_search *is)
{
  xfree(is->buf);
  xfree(is->heap);
}

static void
image_search_grow_slow(struct image_search *is)
{
  is->size *= 2;
  is->buf = xrealloc(is->buf, (is->size + 1) * sizeof(struct image_search_item));
  is->heap = xrealloc(is->heap, (is->size + 1) * sizeof(u32));
}

static inline struct image_search_item *
image_search_grow(struct image_search *is)
{
  if (is->count == is->visited)
  {
    if (is->count == is->size)
      image_search_grow_slow(is);
    is->visited++;
    is->heap[is->visited] = is->visited;
  }
  return is->buf + is->heap[++is->count];
}

static inline uns
image_search_leaf_dist(struct image_search *is, struct image_bbox *bbox, struct image_leaf *leaf)
{
  uns dist = 0;
  uns flags = leaf->flags; 
  for (uns i = 0; i < IMAGE_VEC_K; i++)
    {
      uns bits = IMAGE_LEAF_BITS(i);
      uns mask = (1 << bits) - 1;
      uns value = flags & mask;
      flags >>= bits;
      int dif = bbox->vec[0].f[i] + (bbox->vec[1].f[i] - bbox->vec[0].f[i]) * value / ((1 << bits) - 1) - is->query.f[i];
      dist += dif * dif;
    }
  return dist;
}

static int
image_search_next(struct image_search *is, oid_t *oid, uns *dist)
{
  while (likely(is->count))
    {
      struct image_search_item *item = is->buf + is->heap[1];
      DBG("Main loop... dist=%d count=%d visited=%d size=%d index=0x%08x bbox=[(%s),(%s)]", 
	  item->dist, is->count, is->visited, is->size, item->index, 
	  stk_print_image_vector(&item->bbox.vec[0]), stk_print_image_vector(&item->bbox.vec[1]));
      if (unlikely(item->dist > is->max_dist))
        {
	  DBG("Maximum distance reached");
	  return 0;
	}
      
      /* Expand leaf */
      if (item->index & IMAGE_SEARCH_ITEM_TYPE)
        {
	  *oid = item->index & ~IMAGE_SEARCH_ITEM_TYPE;
	  *dist = item->dist;
	  DBG("Found item %d at distance %d", *oid, *dist);
	  HEAP_DELMIN(u32, is->heap, is->count, IMAGE_SEARCH_CMP, HEAP_SWAP);
	  return 1;
	}
      
      /* Expand node with leaves */
      else if (is->nodes[item->index].val & IMAGE_NODE_LEAF)
        {
	  DBG("Expanding node to list of leaves");
	  struct image_leaf *leaf = is->leaves + (is->nodes[item->index].val & ~IMAGE_NODE_LEAF);
	  item->dist = image_search_leaf_dist(is, &item->bbox, leaf);
	  item->index = IMAGE_SEARCH_ITEM_TYPE | leaf->oid;
	  HEAP_INCREASE(u32, is->heap, is->count, IMAGE_SEARCH_CMP, HEAP_SWAP, 1);
	  while (!((leaf++)->flags & IMAGE_LEAF_LAST))
	    {
	      struct image_search_item *nitem = image_search_grow(is);
	      nitem->dist = image_search_leaf_dist(is, &item->bbox, leaf);
	      nitem->index = IMAGE_SEARCH_ITEM_TYPE | leaf->oid;
	      HEAP_INSERT(u32, is->heap, is->count, IMAGE_SEARCH_CMP, HEAP_SWAP);
	    }
	}

      /* Expand internal node */
      else
        {
	  DBG("Expanding internal node");
	  struct image_search_item *nitem = image_search_grow(is);
	  uns dim = is->nodes[item->index].val & IMAGE_NODE_DIM;
	  uns pivot = is->nodes[item->index].val >> 8;
	  item->index *= 2;
	  nitem->bbox = item->bbox;
	  nitem->dist = item->dist;
	  uns query = is->query.f[dim];
	  int dif = query - pivot;
	  if (dif > 0)
	    {
	      nitem->index = item->index++;
	      item->bbox.vec[0].f[dim] = pivot;
	      nitem->bbox.vec[1].f[dim] = pivot;
	      if (query > item->bbox.vec[1].f[dim])
		nitem->dist -= SQR(query - item->bbox.vec[1].f[dim]);
	    }
	  else
	    {
	      nitem->index = item->index + 1;
	      item->bbox.vec[1].f[dim] = pivot;
	      nitem->bbox.vec[0].f[dim] = pivot;
	      if (query < item->bbox.vec[0].f[dim])
		nitem->dist -= SQR(item->bbox.vec[0].f[dim] - query);
	    }
	  nitem->dist += SQR(dif);
	  HEAP_INSERT(u32, is->heap, is->count, IMAGE_SEARCH_CMP, HEAP_SWAP);
	}
    }
  DBG("Heap is empty");
  return 0;
}

