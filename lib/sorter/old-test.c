/*
 *	UCW Library -- Testing the Old Sorter
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/getopt.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"
#include "lib/ff-binary.h"
#include "lib/hashfunc.h"
#include "lib/md5.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/*** Time measurement ***/

static timestamp_t timer;

static void
start(void)
{
  sync();
  init_timer(&timer);
}

static void
stop(void)
{
  sync();
  log(L_INFO, "Test took %.3fs", get_timer(&timer) / 1000.);
}

/*** Simple 4-byte integer keys ***/

struct key1 {
  u32 x;
};

static inline int s1_compare(struct key1 *x, struct key1 *y)
{
  COMPARE(x->x, y->x);
  return 0;
}

#define SORT_KEY struct key1
#define SORT_PREFIX(x) s1_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
#define SORT_UNIQUE
#define SORT_REGULAR
#define SORT_PRESORT

#include "lib/sorter.h"

static void
test_int(int mode, u64 size)
{
  uns N = size ? nextprime(MIN(size/4, 0xffff0000)) : 0;
  uns K = N/4*3;
  log(L_INFO, ">>> Integers (%s, N=%u)", ((char *[]) { "increasing", "decreasing", "random" })[mode], N);

  struct fastbuf *f = bopen_tmp(65536);
  for (uns i=0; i<N; i++)
    bputl(f, (mode==0) ? i : (mode==1) ? N-1-i : ((u64)i * K + 17) % N);
  brewind(f);

  start();
  f = s1_sort(f);
  stop();

  SORT_XTRACE(2, "Verifying");
  for (uns i=0; i<N; i++)
    {
      uns j = bgetl(f);
      if (i != j)
	die("Discrepancy: %u instead of %u", j, i);
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

#define SORT_KEY struct key3
#define SORT_PREFIX(x) s3_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
#define SORT_REGULAR
#define SORT_PRESORT

#include "lib/sorter.h"

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
test_hashes(int mode, u64 size)
{
  uns N = MIN(size / sizeof(struct key3), 0xffffffff);
  log(L_INFO, ">>> Hashes (%s, N=%u)", ((char *[]) { "increasing", "decreasing", "random" })[mode], N);
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

  start();
  f = s3_sort(f);
  stop();

  SORT_XTRACE(2, "Verifying");
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

static inline int s4_fetch_key(struct fastbuf *f, struct key4 *x)
{
  int len = bgetl(f);
  if (len < 0)
    return 0;
  x->len = len;
  breadb(f, x->s, len);
  return 1;
}

static inline void s4_copy_data(struct fastbuf *i UNUSED, struct fastbuf *f, struct key4 *x)
{
  bputl(f, x->len);
  bwrite(f, x->s, x->len);
}

static inline int s4_compare(struct key4 *x, struct key4 *y)
{
  uns l = MIN(x->len, y->len);
  int c = memcmp(x->s, y->s, l);
  if (c)
    return c;
  COMPARE(x->len, y->len);
  return 0;
}

static inline byte *s4_fetch_item(struct fastbuf *f UNUSED, struct key4 *x, byte *limit UNUSED)
{
  return &x->s[x->len];
}

static inline void s4_store_item(struct fastbuf *f, struct key4 *x)
{
  s4_copy_data(NULL, f, x);
}

#define SORT_KEY struct key4
#define SORT_PREFIX(x) s4_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
#define SORT_PRESORT

#include "lib/sorter.h"

#define s4b_compare s4_compare
#define s4b_fetch_key s4_fetch_key

static inline uns s4_data_size(struct key4 *x)
{
  return x->len ? (x->s[0] ^ 0xad) : 0;
}

static inline void s4b_copy_data(struct fastbuf *i, struct fastbuf *f, struct key4 *x)
{
  bputl(f, x->len);
  bwrite(f, x->s, x->len);
  bbcopy(i, f, s4_data_size(x));
}

static inline byte *s4b_fetch_item(struct fastbuf *f, struct key4 *x, byte *limit)
{
  byte *d = &x->s[x->len];
  if (d + s4_data_size(x) > limit)
    return NULL;
  breadb(f, d, s4_data_size(x));
  return d + s4_data_size(x);
}

static inline void s4b_store_item(struct fastbuf *f, struct key4 *x)
{
  bputl(f, x->len);
  bwrite(f, x->s, x->len + s4_data_size(x));
}

#define SORT_KEY struct key4
#define SORT_PREFIX(x) s4b_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
#define SORT_PRESORT

#include "lib/sorter.h"

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
test_strings(uns mode, u64 size)
{
  uns avg_item_size = KEY4_MAX/2 + 4 + (mode ? 128 : 0);
  uns N = MIN(size / avg_item_size, 0xffffffff);
  log(L_INFO, ">>> Strings %s(N=%u)", (mode ? "with data " : ""), N);
  srand(1);

  struct key4 k, lastk;
  byte buf[256], buf2[256];
  uns sum = 0;

  struct fastbuf *f = bopen_tmp(65536);
  for (uns i=0; i<N; i++)
    {
      gen_key4(&k);
      s4_copy_data(NULL, f, &k);
      uns h = hash_block(k.s, k.len);
      sum += h;
      if (mode)
	{
	  gen_data4(buf, s4_data_size(&k), h);
	  bwrite(f, buf, s4_data_size(&k));
	}
    }
  brewind(f);

  start();
  f = (mode ? s4b_sort : s4_sort)(f);
  stop();

  SORT_XTRACE(2, "Verifying");
  for (uns i=0; i<N; i++)
    {
      int ok = s4_fetch_key(f, &k);
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

/*** Main ***/

static void
run_test(uns i, u64 size)
{
  switch (i)
    {
    case 0:
      test_int(0, size); break;
    case 1:
      test_int(1, size); break;
    case 2:
      test_int(2, size); break;
    case 3:
    case 4:
    case 5:
      break;
    case 6:
      test_hashes(0, size); break;
    case 7:
      test_hashes(1, size); break;
    case 8:
      test_hashes(2, size); break;
    case 9:
      test_strings(0, size); break;
    case 10:
      test_strings(1, size); break;
#define TMAX 11
    }
}

int
main(int argc, char **argv)
{
  log_init(NULL);
  int c;
  u64 size = 10000000;
  uns t = ~0;

  while ((c = cf_getopt(argc, argv, CF_SHORT_OPTS "s:t:v", CF_NO_LONG_OPTS, NULL)) >= 0)
    switch (c)
      {
      case 's':
	if (cf_parse_u64(optarg, &size))
	  goto usage;
	break;
      case 't':
	t = atol(optarg);
	if (t >= TMAX)
	  goto usage;
	break;
      case 'v':
	sorter_trace++;
	break;
      default:
      usage:
	fputs("Usage: sort-test [-v] [-s <size>] [-t <test>]\n", stderr);
	exit(1);
      }
  if (optind != argc)
    goto usage;

  if (t != ~0U)
    run_test(t, size);
  else
    for (uns i=0; i<TMAX; i++)
      run_test(i, size);

  return 0;
}
