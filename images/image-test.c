#define LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include "images/image-search.h"
#include "sherlock/index.h"
#include "lib/mempool.h"
#include "sherlock/object.h"
#include "sherlock/lizard-fb.h"
#include "sherlock/lizard-fb.h"
#include <fcntl.h>
#include <stdio.h>

#define BEST_CNT 20

int
main(int argc, char **argv)
{
  struct image_vector query;
  if (argc != IMAGE_VEC_K + 1)
    die("Invalid number of arguments");

  for (uns i = 0; i < IMAGE_VEC_K; i++)
    {
      uns v;
      if (sscanf(argv[i + 1], "%d", &v) != 1)
	die("Invalid numeric format");
      query.f[i] = v;
    }
  
  
  struct image_search is;
  oid_t best[BEST_CNT];
  uns dist[BEST_CNT];
  uns cardpos[BEST_CNT];
  uns best_n = 0;
  
  image_tree_init();
  log(L_INFO, "Executing query (%s)", stk_print_image_vector(&query));
  image_search_init(&is, &image_tree, &query, IMAGE_SEARCH_DIST_UNLIMITED);
  for (uns i = 0; i < BEST_CNT; i++)
    {
      if (!image_search_next(&is, best + i, dist + i))
        {
          log(L_INFO, "No more images");
          break;
        }
      log(L_INFO, "*** Found %d. best image with oid=%d", i + 1, best[i]);
      best_n++;
    }
  image_search_done(&is);
  image_tree_done();

  log(L_INFO, "Resolving URLs");
  struct mempool *pool = mp_new(1 << 16);
  struct buck2obj_buf *bob = buck2obj_alloc();
  struct fastbuf *fb = bopen("index/card-attrs", O_RDONLY, 1 << 10);
  for (uns i = 0; i < best_n; i++)
    {
      bsetpos(fb, best[i] * sizeof(struct card_attr));
      struct card_attr ca;
      bread(fb, &ca, sizeof(ca));
      cardpos[i] = ca.card;
    }
  bclose(fb);
  fb = bopen("index/cards", O_RDONLY, 1 << 14);
  for (uns i = 0; i < best_n; i++)
    {
      bsetpos(fb, (sh_off_t)cardpos[i] << CARD_POS_SHIFT);
      uns buck_len = bgetl(fb) - (LIZARD_COMPRESS_HEADER - 1);
      uns buck_type = bgetc(fb) + BUCKET_TYPE_PLAIN;
      mp_flush(pool);
      struct odes *obj = obj_read_bucket(bob, pool, buck_type, buck_len, fb, NULL);

      printf("%2d. match: dist=%-8d oid=%-8d url=%s\n", i + 1, dist[i], best[i],
	  obj_find_aval(obj_find_attr(obj, 'U' + OBJ_ATTR_SON)->son, 'U'));
    }
  bclose(fb);
  buck2obj_free(bob);
  mp_delete(pool);
     
  return 0;
}

