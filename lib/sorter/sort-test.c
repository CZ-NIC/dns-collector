/*
 *	UCW Library -- Testing the Sorter
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/getopt.h"
#include "lib/fastbuf.h"
#include "lib/md5.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

/*** Simple 4-byte integer keys ***/

struct key1 {
  u32 x;
};

#define SORT_KEY_REGULAR struct key1
#define SORT_PREFIX(x) s1_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
#define SORT_UNIQUE
#define SORT_INT(k) (k).x

#include "lib/sorter/sorter.h"

static void
test_int(int mode, uns N)
{
  N = nextprime(N);
  uns K = N/4*3;
  log(L_INFO, "Integers (%s, N=%d)", ((char *[]) { "increasing", "decreasing", "random" })[mode], N);

  struct fastbuf *f = bopen_tmp(65536);
  for (uns i=0; i<N; i++)
    bputl(f, (mode==0) ? i : (mode==1) ? N-1-i : ((u64)i * K + 17) % N);
  brewind(f);

  log(L_INFO, "Sorting");
  f = s1_sort(f, NULL, N-1);

  log(L_INFO, "Verifying");
  for (uns i=0; i<N; i++)
    {
      uns j = bgetl(f);
      if (i != j)
	die("Discrepancy: %d instead of %d", j, i);
    }
  bclose(f);
}

/*** Integers with merging, but no data ***/

struct key2 {
  u32 x;
  u32 cnt;
};

static inline void s2_write_merged(struct fastbuf *f, struct key2 **k, void **d UNUSED, uns n, void *buf UNUSED)
{
  for (uns i=1; i<n; i++)
    k[0]->cnt += k[i]->cnt;
  bwrite(f, k[0], sizeof(struct key2));
}

static inline void s2_copy_merged(struct key2 **k, struct fastbuf **d UNUSED, uns n, struct fastbuf *dest)
{
  for (uns i=1; i<n; i++)
    k[0]->cnt += k[i]->cnt;
  bwrite(dest, k[0], sizeof(struct key2));
}

#define SORT_KEY_REGULAR struct key2
#define SORT_PREFIX(x) s2_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
#define SORT_UNIFY
#define SORT_INT(k) (k).x

#include "lib/sorter/sorter.h"

static void
test_counted(int mode, uns N)
{
  N = nextprime(N/4);
  uns K = N/4*3;
  log(L_INFO, "Counted integers (%s, N=%d)", ((char *[]) { "increasing", "decreasing", "random" })[mode], N);

  struct fastbuf *f = bopen_tmp(65536);
  for (uns i=0; i<2*N; i++)
    for (uns j=0; j<2; j++)
      {
	bputl(f, (mode==0) ? (i%N) : (mode==1) ? N-1-(i%N) : ((u64)i * K + 17) % N);
	bputl(f, 1);
      }
  brewind(f);

  log(L_INFO, "Sorting");
  f = s2_sort(f, NULL, N-1);

  log(L_INFO, "Verifying");
  for (uns i=0; i<N; i++)
    {
      uns j = bgetl(f);
      if (i != j)
	die("Discrepancy: %d instead of %d", j, i);
      uns k = bgetl(f);
      if (k != 4)
	die("Discrepancy: %d has count %d instead of 4", j, k);
    }
  bclose(f);
}

/*** Longer records with hashes (similar to Shepherd's index records) ***/

struct key3 {
  u32 hash[4];
  u32 i;
  u32 payload[3];
};

static inline int s3_compare(struct key3 *x, struct key3 *y)
{
  /* FIXME: Maybe unroll manually? */
  for (uns i=0; i<4; i++)
    if (x->hash[i] < y->hash[i])
      return -1;
    else if (x->hash[i] > y->hash[i])
      return 1;
  return 0;
}

static inline uns s3_hash(struct key3 *x)
{
  return x->hash[0];
}

#define SORT_KEY_REGULAR struct key3
#define SORT_PREFIX(x) s3_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
#define SORT_HASH_BITS 32

#include "lib/sorter/sorter.h"

static void
gen_hash_key(int mode, struct key3 *k, uns i)
{
  k->i = i;
  k->payload[0] = 7*i + 13;
  k->payload[1] = 13*i + 19;
  k->payload[2] = 19*i + 7;
  switch (mode)
    {
    case 0:
      k->hash[0] = i;
      k->hash[1] = k->payload[0];
      k->hash[2] = k->payload[1];
      k->hash[3] = k->payload[2];
      break;
    case 1:
      k->hash[0] = ~i;
      k->hash[1] = k->payload[0];
      k->hash[2] = k->payload[1];
      k->hash[3] = k->payload[2];
      break;
    default: ;
      struct MD5Context ctx;
      MD5Init(&ctx);
      MD5Update(&ctx, (byte*) &k->i, 4);
      MD5Final((byte*) &k->hash, &ctx);
      break;
    }
}

static void
test_hashes(int mode, uns N)
{
  log(L_INFO, "Hashes (%s, N=%d)", ((char *[]) { "increasing", "decreasing", "random" })[mode], N);
  struct key3 k, lastk;

  struct fastbuf *f = bopen_tmp(65536);
  uns hash_sum = 0;
  for (uns i=0; i<N; i++)
    {
      gen_hash_key(mode, &k, i);
      hash_sum += k.hash[3];
      bwrite(f, &k, sizeof(k));
    }
  brewind(f);

  log(L_INFO, "Sorting");
  f = s3_sort(f, NULL);

  log(L_INFO, "Verifying");
  for (uns i=0; i<N; i++)
    {
      int ok = breadb(f, &k, sizeof(k));
      ASSERT(ok);
      if (i && s3_compare(&k, &lastk) <= 0)
	ASSERT(0);
      gen_hash_key(mode, &lastk, k.i);
      if (memcmp(&k, &lastk, sizeof(k)))
	ASSERT(0);
      hash_sum -= k.hash[3];
    }
  ASSERT(!hash_sum);
  bclose(f);
}

int
main(int argc, char **argv)
{
  log_init(NULL);
  if (cf_getopt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) >= 0 ||
      optind != argc)
  {
    fputs("This program supports only the following command-line arguments:\n" CF_USAGE, stderr);
    exit(1);
  }

  uns N = 1000000;
  test_int(0, N);
  test_int(1, N);
  test_int(2, N);
  test_counted(0, N);
  test_counted(1, N);
  test_counted(2, N);
  test_hashes(0, N);
  test_hashes(1, N);
  test_hashes(2, N);

  return 0;
}
