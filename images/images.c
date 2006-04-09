#define LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include "sherlock/index.h"

#include <stdio.h>
#include <fcntl.h>

struct image_tree image_tree;

void
image_tree_init(void)
{
  DBG("Initializing image search structures");
  struct fastbuf *fb = bopen("index/image-tree", O_RDONLY, 1 << 16); /* FIXME: filename hack */
  image_tree.count = bgetl(fb);
  image_tree.depth = bgetl(fb);
  ASSERT(image_tree.count < 0x80000000 && image_tree.depth > 0 && image_tree.depth < 30);
  image_tree.nodes = xmalloc((1 << image_tree.depth) * sizeof(struct image_node));
  image_tree.leaves = xmalloc(image_tree.count * sizeof(struct image_leaf));
  bread(fb, &image_tree.bbox, sizeof(struct image_bbox));
  bread(fb, image_tree.nodes + 1, ((1 << image_tree.depth) - 1) * sizeof(struct image_node));
  bread(fb, image_tree.leaves, image_tree.count * sizeof(struct image_leaf));
  DBG("Search tree with depth %d and %d leaves loaded", image_tree.depth, image_tree.count);
  bclose(fb);
}

void
image_tree_done(void)
{
  DBG("Freeing image search structures");
  xfree(image_tree.nodes);
  xfree(image_tree.leaves);
}

