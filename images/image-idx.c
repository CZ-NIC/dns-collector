#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/mempool.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"
#include "lib/chartype.h"
#include "sherlock/object.h"
#include "lib/url.h"
#include "lib/unicode.h"
#include "sherlock/lizard-fb.h"
#include "sherlock/tagged-text.h"
#include "charset/charconv.h"
#include "charset/unicat.h"
#include "charset/fb-charconv.h"
#include "indexer/indexer.h"
#include "indexer/lexicon.h"
#include "indexer/params.h"
#include "utils/dumpconfig.h"
#include "lang/lang.h"
#include "lib/base224.h"
#include "lib/bbuf.h"
#include "lib/clists.h"

#include "images/images.h"
#include "images/image-obj.h"
#include "images/image-sig.h"
#include "images/dup-cmp.h"
#include "images/kd-tree.h"
#include "images/color.h"

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

static struct fastbuf *fb_cards;
static struct fastbuf *fb_card_attrs;
static struct buck2obj_buf *buck2obj;

/* This should happen in gatherer or scanner */
static void
generate_signatures(uns limit)
{
  fb_cards = index_bopen("cards", O_RDONLY);
  fb_card_attrs = index_bopen("card-attrs", O_RDONLY);
  struct fastbuf *fb_signatures = index_bopen("image-sig", O_CREAT | O_WRONLY | O_TRUNC);
  struct card_attr ca;
  struct image_signature sig;
  struct mempool *pool = mp_new(1 << 16);
  struct buck2obj_buf *bob = buck2obj_alloc();
  uns count = 0;

  if (limit == ~0U)
    log(L_INFO, "Generating image signatures");
  else
    log(L_INFO, "Generating at most %d image signatures", limit);
  bputl(fb_signatures, 0);
  imo_decompress_thumbnails_init();

  for (oid_t oid = 0; bread(fb_card_attrs, &ca, sizeof(ca)); oid++)
    if ((uns)((ca.type_flags >> 4) - 8) < 4)
      {
        bsetpos(fb_cards, (sh_off_t)ca.card << CARD_POS_SHIFT);
        uns buck_len = bgetl(fb_cards) - (LIZARD_COMPRESS_HEADER - 1);
        uns buck_type = bgetc(fb_cards) + BUCKET_TYPE_PLAIN;
        mp_flush(pool);
        struct odes *obj = obj_read_bucket(bob, pool, buck_type, buck_len, fb_cards, NULL);
        struct oattr *attr;
        if (!obj)
          die("Failed to read card");
        if (attr = obj_find_attr(obj, 'N'))
          {
#ifdef LOCAL_DEBUG	    
	    byte *url = obj_find_aval(obj_find_attr(obj, 'U' + OBJ_ATTR_SON)->son, 'U');
	    DBG("Reading oid=%d url=%s", oid, url);
#endif	    
	    struct image_obj imo;
	    imo_init(&imo, pool, obj);
	    if (imo_decompress_thumbnail(&imo))
	      {
	        if (compute_image_signature(&imo.thumb, &sig))
	          {
		    bwrite(fb_signatures, &oid, sizeof(oid));
		    bwrite(fb_signatures, &sig.vec, sizeof(struct image_vector));
		    bputc(fb_signatures, sig.len);
		    if (sig.len)
		      bwrite(fb_signatures, sig.reg, sig.len * sizeof(struct image_region));
		    count++;
		    if (count % 10000 == 0)
		      log(L_DEBUG, "... passed %d images", count);
		    if (count >= limit)
	              break;
	          }
	        else
	          DBG("Cannot create signature");
	      }
	    else
	      DBG("Cannot decompress thumbnail");
	  }
      }
  brewind(fb_signatures);
  bputl(fb_signatures, count);
  DBG("%d signatures written", count);

  imo_decompress_thumbnails_done();
  buck2obj_free(bob);
  mp_delete(pool);
  bclose(fb_cards);
  bclose(fb_card_attrs);
  bclose(fb_signatures);
}

struct signature_record {
  oid_t oid;
  struct image_vector vec;
};

#define ASORT_PREFIX(x) build_search_tree_##x
#define ASORT_KEY_TYPE struct signature_record *
#define ASORT_ELT(i) rec[i]
#define ASORT_LT(x,y) x->vec.f[dim] < y->vec.f[dim]
#define ASORT_EXTRA_ARGS , uns dim, struct signature_record **rec
#include "lib/arraysort.h"

#if 0
#define DBG_KD(x...) DBG(x)
#else
#define DBG_KD(x...) do{}while(0)
#endif

static struct image_tree tree;
static struct signature_record *records;
static struct signature_record **precords;

static void
build_kd_tree(void)
{
  log(L_INFO, "Building KD-tree");

  struct fastbuf *fb_signatures = index_bopen("image-sig", O_RDONLY);
  tree.count = bgetl(fb_signatures);
  ASSERT(tree.count < 0x80000000);
  if (!tree.count)
    {
      /* FIXME */
      bclose(fb_signatures);
      die("There are no signatures");
    }
  else
    {
      DBG("Reading %d signatures", tree.count);
      records = xmalloc(tree.count * sizeof(struct signature_record));
      precords = xmalloc(tree.count * sizeof(void *));
      for (uns i = 0; i < tree.count; i++)
        {
	  bread(fb_signatures, &records[i].oid, sizeof(oid_t));
	  bread(fb_signatures, &records[i].vec, sizeof(struct image_vector));
	  uns len = bgetc(fb_signatures);
	  bskip(fb_signatures, len * sizeof(struct image_region));
	  precords[i] = records + i;
	  if (likely(i))
	    for (uns j = 0; j < IMAGE_VEC_K; j++)
	      {
	        tree.bbox.vec[0].f[j] = MIN(tree.bbox.vec[0].f[j], records[i].vec.f[j]);
	        tree.bbox.vec[1].f[j] = MAX(tree.bbox.vec[1].f[j], records[i].vec.f[j]);
	      }
	  else
            tree.bbox.vec[0] = tree.bbox.vec[1] = records[0].vec;
	}
      bclose(fb_signatures);

      for (tree.depth = 1; (uns)(2 << tree.depth) < tree.count; tree.depth++);
      DBG("depth=%d nodes=%d bbox=[(%s), (%s)]", tree.depth, 1 << tree.depth,
	  stk_print_image_vector(tree.bbox.vec + 0), stk_print_image_vector(tree.bbox.vec + 1));
      uns leaves_index = 1 << (tree.depth - 1);
      tree.nodes = xmalloc_zero((1 << tree.depth) * sizeof(struct image_node));
      tree.leaves = xmalloc_zero(tree.count * sizeof(struct image_leaf));

      /* Initialize recursion */
      struct stk {
	struct image_bbox bbox;
	uns index, count;
	struct signature_record **start;
      } stk_top[32], *stk = stk_top + 1;
      stk->index = 1;
      stk->start = precords;
      stk->count = tree.count;
      stk->bbox.vec[0] = tree.bbox.vec[0];
      for (uns i = 0; i < IMAGE_VEC_K; i++)
	stk->bbox.vec[1].f[i] = tree.bbox.vec[1].f[i] - tree.bbox.vec[0].f[i];
      uns entry_index = 0;

      /* Main loop */
      while (stk != stk_top)
        {
	  DBG_KD("Main loop... depth=%d index=%d count=%d, start=%d, min=%s dif=%s",
	      stk - stk_top, stk->index, stk->count, stk->start - precords,
	      stk_print_image_vector(stk->bbox.vec + 0), stk_print_image_vector(stk->bbox.vec + 1));
	  ASSERT(stk->count);

	  /* Create leaf node */
	  if (stk->index >= leaves_index || stk->count < 2)
	    {
	      tree.nodes[stk->index].val = IMAGE_NODE_LEAF | entry_index;
	      for (; stk->count--; stk->start++)
	        {
		  struct image_leaf *leaf = &tree.leaves[entry_index++];
		  struct signature_record *record = *stk->start;
		  leaf->oid = record->oid;
		  leaf->flags = 0;
		  for (uns i = IMAGE_VEC_K; i--; )
		    {
		      uns bits = IMAGE_LEAF_BITS(i);
		      leaf->flags <<= bits;
		      if (stk->bbox.vec[1].f[i])
		        {
		          uns value =
			    (record->vec.f[i] - stk->bbox.vec[0].f[i]) *
			    ((1 << bits) - 1) / stk->bbox.vec[1].f[i];
			  ASSERT(value < (uns)(1 << bits));
			  leaf->flags |= value;
			}
		    }
		  if (!stk->count)
		    leaf->flags |= IMAGE_LEAF_LAST;
	          DBG_KD("Creating leaf node; oid=%d vec=(%s) flags=0x%08x",
		      leaf->oid, stk_print_image_vector(&record->vec), leaf->flags);
		}
	      stk--;
	    }

	  /* Create internal node */
	  else
	    {
	      /* Select dimension to splis */
	      uns dim = 0;
	      for (uns i = 1; i < IMAGE_VEC_K; i++)
		if (stk->bbox.vec[1].f[i] > stk->bbox.vec[1].f[dim])
		  dim = i;

	      /* Sort... FIXME: we only need the median */
	      build_search_tree_sort(stk->count, dim, stk->start);

	      /* Split in the middle */
	      uns index = stk->index;
	      stk[1].index = stk[0].index * 2;
	      stk[0].index = stk[1].index + 1;
	      stk[1].count = stk[0].count >> 1;
	      stk[0].count -= stk[1].count;
	      stk[1].start = stk[0].start;
	      stk[0].start += stk[1].count;

	      /* Choose split value */
	      uns lval = stk->start[-1]->vec.f[dim];
	      uns rval = stk->start[0]->vec.f[dim];
	      uns pivot = stk->bbox.vec[0].f[dim] + (stk->bbox.vec[1].f[dim] >> 1);
	      if (pivot <= lval)
		pivot = lval;
	      else if (pivot >= rval)
		pivot = rval;

	      DBG_KD("Created internal node; dim=%d pivot=%d", dim, pivot);

	      /* Split the box */
	      stk[1].bbox = stk[0].bbox;
              stk[1].bbox.vec[1].f[dim] = pivot - stk[0].bbox.vec[0].f[dim];
	      stk[0].bbox.vec[0].f[dim] += stk[1].bbox.vec[1].f[dim];
	      stk[0].bbox.vec[1].f[dim] -= stk[1].bbox.vec[1].f[dim];

	      /* Fill the node structure */
	      tree.nodes[index].val = dim + (pivot << 8);
	      stk++;
	    }
	}

      DBG("Tree constructed, saving...");

      struct fastbuf *fb_tree = index_bopen("image-tree", O_CREAT | O_WRONLY | O_TRUNC);
      bputl(fb_tree, tree.count);
      bputl(fb_tree, tree.depth);
      bwrite(fb_tree, &tree.bbox, sizeof(struct image_bbox));
      bwrite(fb_tree, tree.nodes + 1, ((1 << tree.depth) - 1) * sizeof(struct image_node));
      bwrite(fb_tree, tree.leaves, tree.count * sizeof(struct image_leaf));
      bclose(fb_tree);

      //xfree(tree.leaves);
      //xfree(tree.nodes);
      //xfree(precords);
      //xfree(records);
    }
}

/*********************************************************************************/

static uns pass1_buf_size = 400 << 20;
static uns pass1_max_count = 100000;
static uns pass1_search_dist = 40;
static uns pass1_search_count = 500;

static struct mempool *pass1_pool;
static byte *pass1_buf_start;
static byte *pass1_buf_pos;
static uns pass1_buf_free;
static uns pass1_buf_used;
static clist pass1_buf_list;
static clist pass1_lru_list;
static u64 pass1_lookups;
static u64 pass1_reads;
static u64 pass1_pairs;
static u64 pass1_dups;
static u64 pass1_shrinks;

struct pass1_node {
  cnode lru_node;
  cnode buf_node;
  uns buf_size;
  byte *buf;
  oid_t oid;
  byte *url;
  struct image image;
  struct image_dup dup;
};

#define HASH_PREFIX(x) pass1_hash_##x
#define HASH_NODE struct pass1_node
#define HASH_KEY_ATOMIC oid
#define HASH_WANT_CLEANUP
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_WANT_REMOVE
#include "lib/hashtable.h"

static inline void
pass1_buf_init(void)
{
  //DBG("pass1_buf_init()");
  pass1_buf_free = pass1_buf_size;
  pass1_buf_start = pass1_buf_pos = xmalloc(pass1_buf_size);
  pass1_buf_used = 0;
}

static inline void
pass1_buf_cleanup(void)
{
  //DBG("pass1_buf_cleanup()");
  xfree(pass1_buf_start);
}

static void
pass1_node_free(struct pass1_node *node)
{
  //DBG("pass1_node_free(%d)", (uns)node->oid);
  if (node->buf_size)
    {
      pass1_buf_used -= node->buf_size;
      clist_remove(&node->buf_node);
    }
  clist_remove(&node->lru_node);
  pass1_hash_remove(node);
}

static inline void
pass1_node_free_lru(void)
{
  ASSERT(!clist_empty(&pass1_lru_list));
  pass1_node_free(SKIP_BACK(struct pass1_node, lru_node, clist_head(&pass1_lru_list)));
}

static inline void
pass1_node_after_move(struct pass1_node *node, addr_int_t move)
{
  //DBG("pass1_node_after_mode(%d, %d)", (uns)node->oid, (uns)move);
  /* adjust internal pointers */
#define MOVE(x) x = (byte *)(x) - move
  MOVE(node->url);
  MOVE(node->image.pixels);
  MOVE(node->dup.buf);
#undef MOVE
}

static inline void
pass1_buf_shrink(void)
{
  DBG("pass1_buf_shrink()");
  pass1_shrinks++;
  pass1_buf_free = pass1_buf_size;
  pass1_buf_pos = pass1_buf_start;
  CLIST_FOR_EACH(void *, p, pass1_buf_list)
    {
      struct pass1_node *node = SKIP_BACK(struct pass1_node, buf_node, p);
      if (node->buf != pass1_buf_pos)
        {
          memmove(pass1_buf_pos, node->buf, node->buf_size);
          pass1_node_after_move(node, node->buf - pass1_buf_pos);
          node->buf = pass1_buf_pos;
	}
      pass1_buf_pos += node->buf_size;
      pass1_buf_free -= node->buf_size;
    }
}

static void *
pass1_buf_alloc(uns size)
{
  //DBG("pass1_buf_alloc(%d)", size);

  /* if there is not enough free space at the end of the buffer */
  if (size > pass1_buf_free)
    {
      /* free some lru nodes */
      //DBG("freeing lru nodes");
      while (size > pass1_buf_size - pass1_buf_used || pass1_buf_used * 2 > pass1_buf_size)
        {
	  if (unlikely(clist_empty(&pass1_lru_list))) // FIXME
	    die("Buffer too small");
          pass1_node_free_lru();
	}

      pass1_buf_shrink();
    }

  /* final allocation */
  void *result = pass1_buf_pos;
  pass1_buf_pos += size;
  pass1_buf_free -= size;
  pass1_buf_used += size;
  return result;
}

static struct pass1_node *
pass1_node_new(oid_t oid)
{
  DBG("pass1_node_new(%d)", (uns)oid);
  if (pass1_hash_table.hash_count == pass1_max_count)
    pass1_node_free_lru();
  struct pass1_node *node = pass1_hash_new(oid);
  mp_flush(pass1_pool);
  pass1_reads++;

  /* read object */
  struct card_attr ca;
  bsetpos(fb_card_attrs, (sh_off_t)oid * sizeof(ca)); /* FIXME: these seeks can be easily removed */
  bread(fb_card_attrs, &ca, sizeof(ca));

  bsetpos(fb_cards, (sh_off_t)ca.card << CARD_POS_SHIFT); /* FIXME: maybe a presort should handle these random seeks */
  uns buck_len = bgetl(fb_cards) - (LIZARD_COMPRESS_HEADER - 1);
  uns buck_type = bgetc(fb_cards) + BUCKET_TYPE_PLAIN;
  struct odes *obj = obj_read_bucket(buck2obj, pass1_pool, buck_type, buck_len, fb_cards, NULL);
  if (unlikely(!obj))
    die("Failed to read card");
  byte *url = obj_find_aval(obj_find_attr(obj, 'U' + OBJ_ATTR_SON)->son, 'U');
  uns url_len = strlen(url);

  /* decompress thumbnail */
  struct image_obj imo;
  imo_init(&imo, pass1_pool, obj);
  if (unlikely(!imo_decompress_thumbnail(&imo)))
    die("Cannot decompress thumbnail");
  node->image = imo.thumb;

  /* create duplicates comparision object */
  image_dup_init(&node->dup, &node->image, pass1_pool);

  /* copy data */
  //DBG("loaded image %s s=%d d=%d", url, node->image.size, node->dup.buf_size);
  node->buf_size = node->image.size + node->dup.buf_size + url_len + 1;
  if (node->buf_size)
    {
      byte *buf = node->buf = pass1_buf_alloc(node->buf_size);
      clist_add_tail(&pass1_buf_list, &node->buf_node);
#define COPY(ptr, size) ({ void *_p=buf; uns _size=(size); buf+=_size; memcpy(_p,(ptr),_size); _p; })
      node->url = COPY(url, url_len + 1);
      node->image.pixels = COPY(node->image.pixels, node->image.size);
      node->dup.buf = COPY(node->dup.buf, node->dup.buf_size);
#undef COPY
    }

  /* add to lru list */
  return node;
}

static inline struct pass1_node *
pass1_node_lock(oid_t oid)
{
  DBG("pass1_node_lock(%d)", (uns)oid);
  pass1_lookups++;
  struct pass1_node *node = pass1_hash_find(oid);
  if (node)
    {
      clist_remove(&node->lru_node);
      return node;
    }
  else
    return pass1_node_new(oid);
}

static inline void
pass1_node_unlock(struct pass1_node *node)
{
  //DBG("pass1_node_unlock(%d)", (uns)node->oid);
  clist_add_tail(&pass1_lru_list, &node->lru_node);
}

static void
pass1(void)
{
  log(L_INFO, "Looking for duplicates");
  ASSERT(tree.nodes);

  /* initialization */
  pass1_lookups = pass1_reads = pass1_pairs = pass1_dups = pass1_shrinks = 0;
  fb_cards = bopen("index/cards", O_RDONLY, 10000); // FIXME
  fb_card_attrs = bopen("index/card-attrs", O_RDONLY, sizeof(struct card_attr)); // FIXME
  buck2obj = buck2obj_alloc();
  imo_decompress_thumbnails_init();
  clist_init(&pass1_lru_list);
  clist_init(&pass1_buf_list);
  pass1_hash_init();
  pass1_buf_init();
  pass1_pool = mp_new(1 << 20);

  /* main loop */
  for (uns i = 0; i < tree.count; )
    {
      /* lookup next image */
      oid_t oid = tree.leaves[i].oid;
      struct pass1_node *node = pass1_node_lock(oid);

      /* compare with all near images */
      struct image_search search;
      image_search_init(&search, &tree, &precords[i]->vec, pass1_search_dist);
      /* FIXME: can be faster than general search in KD-tree */
      oid_t oid2;
      uns dist;
      for (uns j = 0; j < pass1_search_count && image_search_next(&search, &oid2, &dist); j++)
        {
	  if (oid < oid2)
            {
	      struct pass1_node *node2 = pass1_node_lock(oid2);
	      DBG("comparing %d and %d", oid, oid2);
	      if (image_dup_compare(&node->dup, &node2->dup, IMAGE_DUP_TRANS_ALL))
	        {
		  pass1_dups++;
		  log(L_DEBUG, "*** Found duplicates oid1=0x%x oid=0x%x", (uns)node->oid, (uns)node2->oid);
		  log(L_DEBUG, "  %s", node->url);
		  log(L_DEBUG, "  %s", node2->url);
	        }
	      pass1_pairs++;
	      pass1_node_unlock(node2);
	    }
	}
      image_search_done(&search);
      pass1_node_unlock(node);
      i++;
      if (i % 1000 == 0)
        log(L_DEBUG, "... passed %d images", i);
    }

  /* clean up */
  pass1_hash_cleanup();
  pass1_buf_cleanup();
  mp_delete(pass1_pool);
  bclose(fb_cards);
  bclose(fb_card_attrs);
  buck2obj_free(buck2obj);
  imo_decompress_thumbnails_done();

  /* print statistics */
  log(L_INFO, "%d count, %Ld lookups, %Ld reads, %Ld pairs, %Ld dups, %Ld shrinks", tree.count,
    (long long int)pass1_lookups, (long long int)pass1_reads,
    (long long int)pass1_pairs, (long long int)pass1_dups, (long long int)pass1_shrinks);
}

/*********************************************************************************/

static char *shortopts = CF_SHORT_OPTS "";
static struct option longopts[] =
{
  CF_LONG_OPTS
  { NULL, 0, 0, 0 }
};

static char *help = "\
Usage: image-indexer [<options>]\n\
\n\
Options:\n" CF_USAGE;

static void NONRET
usage(byte *msg)
{
  if (msg)
  {
    fputs(msg, stderr);
    fputc('\n', stderr);
  }
  fputs(help, stderr);
  exit(1);
}


int
main(int argc UNUSED, char **argv)
{
  int opt;

  log_init(argv[0]);
  while ((opt = cf_getopt(argc, argv, shortopts, longopts, NULL)) >= 0)
    switch (opt)
    {
      default:
      usage("Invalid option");
    }
  if (optind != argc)
    usage("Invalid usage");

  srgb_to_luv_init();

  generate_signatures(20000);
  build_kd_tree();
  pass1();

  return 0;
}
