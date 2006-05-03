/* Tests for multidimensional Hilbert curves */

#define LOCAL_DEBUG

#include "lib/lib.h"

#include <stdlib.h>

static uns test1_dim;
static uns test1_order;
#define HILBERT_PREFIX(x) test1_##x
#define HILBERT_DIM test1_dim
#define HILBERT_ORDER test1_order
#define HILBERT_WANT_DECODE
#define HILBERT_WANT_ENCODE
#include "images/hilbert.h"

static void
test1(void)
{
  uns a[32], b[32], c[32];
  for (test1_dim = 2; test1_dim <= 8; test1_dim++)
    for (test1_order = 8; test1_order <= 32; test1_order++)
      for (uns i = 0; i < 1000; i++)
        {
          for (uns j = 0; j < test1_dim; j++)
	    a[j] = (uns)rand() >> (32 - test1_order);
          test1_encode(b, a);
          test1_decode(c, b);
          for (uns j = 0; j < test1_dim; j++)
	    if (a[j] != c[j])
	      die("Error... dim=%d order=%d testnum=%d index=%d val1=0x%08x val2=0x%08x", test1_dim, test1_order, i, j, a[j], c[j]);
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
  test1();
  //test_origin();
  return 0;
}
