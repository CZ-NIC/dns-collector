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
#include "lib/hashfunc.h"
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
    COMPARE(x->hash[i], y->hash[i]);
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

/*** Variable-length records (strings) with and without var-length data ***/

#define KEY4_MAX 256

struct key4 {
  uns len;
  byte s[KEY4_MAX];
};

static inline int s4_compare(struct key4 *x, struct key4 *y)
{
  uns l = MIN(x->len, y->len);
  int c = memcmp(x->s, y->s, l);
  if (c)
    return c;
  COMPARE(x->len, y->len);
  return 0;
}

static inline int s4_read_key(struct fastbuf *f, struct key4 *x)
{
  x->len = bgetl(f);
  if (x->len == 0xffffffff)
    return 0;
  ASSERT(x->len < KEY4_MAX);
  breadb(f, x->s, x->len);
  return 1;
}

static inline void s4_write_key(struct fastbuf *f, struct key4 *x)
{
  ASSERT(x->len < KEY4_MAX);
  bputl(f, x->len);
  bwrite(f, x->s, x->len);
}

#define SORT_KEY struct key4
#define SORT_PREFIX(x) s4_##x
#define SORT_KEY_SIZE(x) (sizeof(struct key4) - KEY4_MAX + (x).len)
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB

#include "lib/sorter/sorter.h"

#define s4b_compare s4_compare
#define s4b_read_key s4_read_key
#define s4b_write_key s4_write_key

static inline uns s4_data_size(struct key4 *x)
{
  return x->len ? (x->s[0] ^ 0xad) : 0;
}

#define SORT_KEY struct key4
#define SORT_PREFIX(x) s4b_##x
#define SORT_KEY_SIZE(x) (sizeof(struct key4) - KEY4_MAX + (x).len)
#define SORT_DATA_SIZE(x) s4_data_size(&(x))
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB

#include "lib/sorter/sorter.h"

static void
gen_key4(struct key4 *k)
{
  k->len = random_max(KEY4_MAX);
  for (uns i=0; i<k->len; i++)
    k->s[i] = random();
}

static void
gen_data4(byte *buf, uns len, uns h)
{
  while (len--)
    {
      *buf++ = h >> 24;
      h = h*259309 + 17;
    }
}

static void
test_strings(uns mode, uns N)
{
  log(L_INFO, "Strings %s(N=%d)", (mode ? "with data " : ""), N);
  srand(1);

  struct key4 k, lastk;
  byte buf[256], buf2[256];
  uns sum = 0;

  struct fastbuf *f = bopen_tmp(65536);
  for (uns i=0; i<N; i++)
    {
      gen_key4(&k);
      s4_write_key(f, &k);
      uns h = hash_block(k.s, k.len);
      sum += h;
      if (mode)
	{
	  gen_data4(buf, s4_data_size(&k), h);
	  bwrite(f, buf, s4_data_size(&k));
	}
    }
  brewind(f);

  log(L_INFO, "Sorting");
  f = (mode ? s4b_sort : s4_sort)(f, NULL);

  log(L_INFO, "Verifying");
  for (uns i=0; i<N; i++)
    {
      int ok = s4_read_key(f, &k);
      ASSERT(ok);
      uns h = hash_block(k.s, k.len);
      if (mode && s4_data_size(&k))
	{
	  ok = breadb(f, buf, s4_data_size(&k));
	  ASSERT(ok);
	  gen_data4(buf2, s4_data_size(&k), h);
	  ASSERT(!memcmp(buf, buf2, s4_data_size(&k)));
	}
      if (i && s4_compare(&k, &lastk) < 0)
	ASSERT(0);
      sum -= h;
      lastk = k;
    }
  ASSERT(!sum);
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

  uns N = 100000;
#if 0
  test_int(0, N);
  test_int(1, N);
  test_int(2, N);
  test_counted(0, N);
  test_counted(1, N);
  test_counted(2, N);
  test_hashes(0, N);
  test_hashes(1, N);
  test_hashes(2, N);
  test_strings(0, N);
#endif
  test_strings(1, N);

  return 0;
}
