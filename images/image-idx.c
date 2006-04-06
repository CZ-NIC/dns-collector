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
  struct fastbuf *cards = index_bopen("cards", O_RDONLY);
  struct fastbuf *card_attrs = index_bopen("card-attrs", O_RDONLY);
  struct fastbuf *signatures = index_bopen("image-sig", O_CREAT | O_WRONLY | O_TRUNC);
  struct card_attr ca;
  struct image_signature sig;
  struct mempool *pool = mp_new(1 << 16);
  struct buck2obj_buf *bob = buck2obj_alloc();
  oid_t oid = 0;

  DBG("Generating signatures");

  for (; bread(card_attrs, &ca, sizeof(ca)); oid++)
    if ((uns)((ca.type_flags >> 4) - 8) < 4)
      {
        bsetpos(cards, (sh_off_t)ca.card << CARD_POS_SHIFT);
        uns buck_len = bgetl(cards)-(LIZARD_COMPRESS_HEADER-1);
        uns buck_type = bgetc(cards) + BUCKET_TYPE_PLAIN;
        mp_flush(pool);
        struct odes *obj = obj_read_bucket(bob, pool, buck_type, buck_len, cards, NULL);
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
	        bputl(signatures, oid);
	        bwrite(signatures, &sig, sizeof(sig));
                if (!--limit)
	          break;
	      }
	    else
	      DBG("Cannot create signature, error=%d", err);

	    bb_done(&buf);
	  }
      }

  buck2obj_free(bob);
  mp_delete(pool);
  bclose(cards);
  bclose(card_attrs);
  bclose(signatures);
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
  
  return 0;
}
