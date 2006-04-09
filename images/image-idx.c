#define LOCAL_DEBUG

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

#include "images/images.h"

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

/* This should happen in gatherer or scanner */
static void
generate_signatures(uns limit)
{
  struct fastbuf *fb_cards = index_bopen("cards", O_RDONLY);
  struct fastbuf *fb_card_attrs = index_bopen("card-attrs", O_RDONLY);
  struct fastbuf *fb_signatures = index_bopen("image-sig", O_CREAT | O_WRONLY | O_TRUNC);
  struct card_attr ca;
  struct image_signature sig;
  struct mempool *pool = mp_new(1 << 16);
  struct buck2obj_buf *bob = buck2obj_alloc();
  uns count = 0;

  log(L_INFO, "Generating image signatures");
  bputl(fb_signatures, 0);
  compute_image_signature_prepare();

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
	    DBG("Reading oid=%d url=%s", oid, obj_find_aval(obj_find_attr(obj, 'U' + OBJ_ATTR_SON)->son, 'U'));
	    bb_t buf;
	    uns buf_len = 0;
	    bb_init(&buf);
	    for (; attr; attr = attr->same)
	      {
	        uns len = strlen(attr->val);
  	        bb_grow(&buf, buf_len + len);
                memcpy(buf.ptr + buf_len, attr->val, len);
	        buf_len += len;
	      }
	    byte thumb[buf_len];
	    uns thumb_len = base224_decode(thumb, buf.ptr, buf_len);
	   
	    int err = compute_image_signature(thumb, thumb_len, &sig);
	    if (!err)
	      {
		bwrite(fb_signatures, &oid, sizeof(oid));
		bwrite(fb_signatures, &sig.vec, sizeof(struct image_vector));
		bputc(fb_signatures, sig.len);
		if (sig.len)
		  bwrite(fb_signatures, sig.reg, sig.len * sizeof(struct image_region));
		count++;
		if (count >= limit)
	          break;
	      }
	    else
	      DBG("Cannot create signature, error=%d", err);

	    bb_done(&buf);
	  }
      }
  brewind(fb_signatures);
  bputl(fb_signatures, count);
  DBG("%d signatures written", count);

  compute_image_signature_finish();
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

static void
build_search_tree(void)
{
  log(L_INFO, "Building KD-tree");

  struct fastbuf *fb_signatures = index_bopen("image-sig", O_RDONLY);
  struct image_tree tree;
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
      struct signature_record *records = xmalloc(tree.count * sizeof(struct signature_record));
      struct signature_record **precords = xmalloc(tree.count * sizeof(void *));
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
      tree.nodes = xmalloc((1 << tree.depth) * sizeof(struct image_node));
      tree.leaves = xmalloc(tree.count * sizeof(struct image_leaf));
     
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
			  ASSERT(value < (1 << bits));
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
      bwrite(fb_tree, tree.nodes + 1, ((1 << tree.depth) - 1) * sizeof(struct image_node));
      bwrite(fb_tree, tree.leaves, tree.count * sizeof(struct image_leaf));
      bclose(fb_tree);

      xfree(tree.leaves);
      xfree(tree.nodes);
      xfree(precords);
      xfree(records);
    }
}


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

  generate_signatures(~0U);
  build_search_tree();
  
  return 0;
}
