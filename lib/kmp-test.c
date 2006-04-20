/*
 *      Test of KMP search
 *
 *      (c) 2006, Pavel Charvat <pchar@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/mempool.h"
#include <string.h>

#if 0
#define TRACE(x...) do{log(L_DEBUG, x);}while(0)
#else
#define TRACE(x...) do{}while(0)
#endif

/* TEST1 - multiple searches */

#define KMP_PREFIX(x) GLUE_(kmp1,x)
#define KMP_WANT_CLEANUP
#include "lib/kmp-new.h"
#define KMPS_PREFIX(x) GLUE_(kmp1s1,x)
#define KMPS_KMP_PREFIX(x) GLUE_(kmp1,x)
#define KMPS_WANT_BEST
#define KMPS_T uns
#define KMPS_EXIT(ctx,src,s) do{ return s.best->len; }while(0)
#include "lib/kmp-search.h"
#define KMPS_PREFIX(x) GLUE_(kmp1s2,x)
#define KMPS_KMP_PREFIX(x) GLUE_(kmp1,x)
#define KMPS_EXTRA_VAR uns
#define KMPS_INIT(ctx,src,s) do{ s.v = 0; }while(0)
#define KMPS_T uns
#define KMPS_FOUND(ctx,src,s) do{ s.v++; }while(0)
#define KMPS_EXIT(ctx,src,s) do{ return s.v; }while(0)
#define KMPS_WANT_BEST
#include "lib/kmp-search.h"

static void
test1(void)
{
  TRACE("Running test1");
  struct kmp1_context ctx;
  kmp1_init(&ctx);
  kmp1_add(&ctx, "ahoj");
  kmp1_add(&ctx, "hoj");
  kmp1_add(&ctx, "aho");
  kmp1_build(&ctx);
  UNUSED uns best = kmp1s1_search(&ctx, "asjlahslhalahosjkjhojsas");
  TRACE("Best match has %d characters", best);
  ASSERT(best == 3);
  UNUSED uns count = kmp1s2_search(&ctx, "asjlahslhalahojsjkjhojsas");
  ASSERT(count == 4);
  kmp1_cleanup(&ctx);
}

/* TEST2 - various tracing */

#define KMP_PREFIX(x) GLUE_(kmp2,x)
#define KMP_USE_UTF8
#define KMP_TOLOWER
#define KMP_ONLYALPHA
#define KMP_NODE struct { byte *str; uns id; }
#define KMP_ADD_EXTRA_ARGS uns id
#define KMP_ADD_EXTRA_VAR byte *
#define KMP_ADD_INIT(ctx,src,var) do{ var = src; }while(0)
#define KMP_ADD_NEW(ctx,src,var,state) do{ TRACE("Inserting string %s with id %d", var, id); \
  state->n.str = var; state->n.id = id; }while(0)
#define KMP_ADD_DUP(ctx,src,var,state) do{ TRACE("String %s already inserted", var); }while(0)
#define KMP_WANT_CLEANUP
#define KMP_WANT_SEARCH
#define KMPS_ADD_CONTROLS
#define KMPS_MERGE_CONTROLS
#define KMPS_WANT_BEST
#define KMPS_FOUND(ctx,src,s) do{ TRACE("String %s with id %d found", s.out->n.str, s.out->n.id); }while(0)
#define KMPS_STEP(ctx,src,s) do{ TRACE("Got to state %p after reading %d", s.s, s.c); }while(0)
#define KMPS_EXIT(ctx,src,s) do{ if (s.best->len) TRACE("Best match is %s", s.best->n.str); } while(0)
#include "lib/kmp-new.h"

static void
test2(void)
{
  TRACE("Running test2");
  struct kmp2_context ctx;
  kmp2_init(&ctx);
  kmp2_add(&ctx, "ahoj", 1);
  kmp2_add(&ctx, "ahoj", 2);
  kmp2_add(&ctx, "hoj", 3);
  kmp2_add(&ctx, "aho", 4);
  kmp2_add(&ctx, "aba", 5);
  kmp2_add(&ctx, "aba", 5);
  kmp2_add(&ctx, "pěl", 5);
  kmp2_build(&ctx);
  kmp2_search(&ctx, "Šíleně žluťoučký kůň úpěl ďábelské ódy labababaks sdahojdhsaladsjhla");
  kmp2_cleanup(&ctx);
}

/* TEST3 - random tests */

#define KMP_PREFIX(x) GLUE_(kmp3,x)
#define KMP_NODE uns
#define KMP_ADD_EXTRA_ARGS uns index
#define KMP_ADD_EXTRA_VAR byte *
#define KMP_ADD_INIT(ctx,src,v) do{ v = src; }while(0)
#define KMP_ADD_NEW(ctx,src,v,s) do{ s->n = index; }while(0)
#define KMP_ADD_DUP(ctx,src,v,s) do{ *v = 0; }while(0)
#define KMP_WANT_CLEANUP
#define KMP_WANT_SEARCH
#define KMPS_EXTRA_ARGS uns *cnt, uns *sum
#define KMPS_FOUND(ctx,src,s) do{ ASSERT(cnt[s.out->n]); cnt[s.out->n]--; sum[0]--; }while(0)
#include "lib/kmp-new.h"

static void
test3(void)
{
  TRACE("Running test3");
  struct mempool *pool = mp_new(1024);
  for (uns testn = 0; testn < 100; testn++)
  {
    mp_flush(pool);
    uns n = random_max(100);
    byte *s[n];
    struct kmp3_context ctx;
    kmp3_init(&ctx);
    for (uns i = 0; i < n; i++)
      {
        uns m = random_max(10);
        s[i] = mp_alloc(pool, m + 1);
        for (uns j = 0; j < m; j++)
	  s[i][j] = 'a' + random_max(3);
        s[i][m] = 0;
        kmp3_add(&ctx, s[i], i);
      }
    kmp3_build(&ctx);
    for (uns i = 0; i < 10; i++)
      {
        uns m = random_max(100);
        byte b[m + 1];
        for (uns j = 0; j < m; j++)
	  b[j] = 'a' + random_max(4);
        b[m] = 0;
        uns cnt[n], sum = 0;
        for (uns j = 0; j < n; j++)
          {
	    cnt[j] = 0;
	    if (*s[j])
	      for (uns k = 0; k < m; k++)
	        if (!strncmp(b + k, s[j], strlen(s[j])))
	          cnt[j]++, sum++;
	  }
        kmp3_search(&ctx, b, cnt, &sum);
        ASSERT(sum == 0);
      }
    kmp3_cleanup(&ctx);
  }
  mp_delete(pool);
}

/* TEST4 - user-defined character type */

struct kmp4_context;
struct kmp4_state;

static inline int
kmp4_eq(struct kmp4_context *ctx UNUSED, byte *a, byte *b)
{
  return (a == b) || (a && b && *a == *b);
}

static inline uns
kmp4_hash(struct kmp4_context *ctx UNUSED, struct kmp4_state *s, byte *c)
{
  return (c ? (*c << 16) : 0) + (uns)(addr_int_t)s;
}

#define KMP_PREFIX(x) GLUE_(kmp4,x)
#define KMP_CHAR byte *
#define KMP_CONTROL_CHAR NULL
#define KMP_GET_CHAR(ctx,src,c) ({ c = src++; !!*c; })
#define KMP_GIVE_HASHFN
#define KMP_GIVE_EQ
#define KMP_WANT_CLEANUP
#define KMP_WANT_SEARCH
#define KMPS_FOUND(ctx,src,s) do{ TRACE("found"); }while(0)
#define KMPS_ADD_CONTROLS
#define KMPS_MERGE_CONTROLS
#include "lib/kmp-new.h"

static void
test4(void)
{
  TRACE("Running test4");
  struct kmp4_context ctx;
  kmp4_init(&ctx);
  kmp4_add(&ctx, "ahoj");
  kmp4_build(&ctx);
  kmp4_search(&ctx, "djdhaskjdahoahaahojojshdaksjahdahojskj");
  kmp4_cleanup(&ctx);
}

int
main(void)
{
  test1();
  test2();
  test3();
  test4();
  return 0;
}
