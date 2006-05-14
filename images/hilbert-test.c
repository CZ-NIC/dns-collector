/* Tests for multidimensional Hilbert curves */

#define LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mempool.h"
#include "lib/math.h"

#include <stdlib.h>
#include <stdio.h>

static struct mempool *pool;

static uns dim;
static uns order;

static inline void
rand_vec(uns *vec)
{
  for (uns i = 0; i < dim; i++)
    vec[i] = (uns)rand() >> (32 - order);
}

static byte *
print_vec(uns *vec)
{
  byte *s = mp_alloc(pool, dim * 16), *res = s;
  *s++ = '(';
  for (uns i = 0; i < dim; i++)
    {
      if (i)
	*s++ = ' ';
      s += sprintf(s, "%x", vec[i]);
    }
  *s++ = ')';
  *s = 0;
  return res;
}

static inline int
cmp_vec(uns *vec1, uns *vec2)
{
  for (uns i = dim; i--; )
    if (vec1[i] < vec2[i])
      return -1;
    else if (vec1[i] > vec2[i])
      return 1;
  return 0;
}

#if 0
static long double
param_dist(uns *vec1, uns *vec2)
{
  long double d1 = 0, d2 = 0;
  for (uns i = 0; i < dim; i++)
    {
      d1 = (d1 + vec1[i]) / ((u64)1 << order);
      d2 = (d2 + vec2[i]) / ((u64)1 << order);
    }
  return fabsl(d1 - d2);
}

static long double
vec_dist(uns *vec1, uns *vec2)
{
  long double d = 0;
  for (uns i = 0; i < dim; i++)
    {
      long double x = fabsl(vec1[i] - vec2[i]) / ((u64)1 << order);
      d += x * x;
    }
  return sqrtl(d);
}
#endif

#define HILBERT_PREFIX(x) test1_##x
#define HILBERT_DIM dim
#define HILBERT_ORDER order
#define HILBERT_WANT_DECODE
#define HILBERT_WANT_ENCODE
#include "images/hilbert.h"

static void
test1(void)
{
  uns a[32], b[32], c[32];
  for (dim = 2; dim <= 8; dim++)
    for (order = 8; order <= 32; order++)
      for (uns i = 0; i < 1000; i++)
        {
	  rand_vec(a);
          test1_encode(b, a);
          test1_decode(c, b);
	  if (cmp_vec(a, c))
	    die("Error... dim=%d order=%d testnum=%d ... %s -> %s -> %s", 
		dim, order, i, print_vec(a), print_vec(b), print_vec(c));
        }
}

#if 0
#include "images/hilbert-origin.h"
static void
test_origin(void)
{
  Hcode code;
  Point pt, pt2;
  pt.hcode[0] = 0x12345678;
  pt.hcode[1] = 0x654321;
  pt.hcode[2] = 0x11122233;
  code = H_encode(pt);
  pt2 = H_decode(code);
  DBG("origin: [%08x, %08x, %08x] --> [%08x, %08x %08x] --> [%08x, %08x %08x]", 
    pt.hcode[0], pt.hcode[1], pt.hcode[2], code.hcode[0], code.hcode[1], code.hcode[2], pt2.hcode[0], pt2.hcode[1], pt2.hcode[2]);
}
#endif

int
main(int argc UNUSED, char **argv UNUSED)
{
  pool = mp_new(1 << 16);
  test1();
  //test_origin();
  return 0;
}
