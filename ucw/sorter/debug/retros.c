/*
 *  Experiments with various sorting algorithms
 *
 *  (c) 2007--2008 Martin Mares <mj@ucw.cz>
 */

#include <ucw/lib.h>
#include <ucw/getopt.h>
#include <ucw/md5.h>
#include <ucw/heap.h>
#include <ucw/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/user.h>

struct elt {
  u32 key;
  u32 ballast[3];
};

static struct elt *ary, *alt, **ind, *array0, *array1;
static uns n = 10000000;
static u32 sum;

static struct elt *alloc_elts(uns n)
{
  return big_alloc(n * sizeof(struct elt));
}

static void free_elts(struct elt *a, uns n)
{
  big_free(a, n * sizeof(struct elt));
}

static int comp(const void *x, const void *y)
{
  const struct elt *xx = x, *yy = y;
  return (xx->key < yy->key) ? -1 : (xx->key > yy->key) ? 1 : 0;
}

static int comp_ind(const void *x, const void *y)
{
  const struct elt * const *xx = x, * const *yy = y;
  return comp(*xx, *yy);
}

#define ASORT_PREFIX(x) as_##x
#define ASORT_KEY_TYPE u32
#define ASORT_ELT(i) a[i].key
#define ASORT_SWAP(i,j) do { struct elt t=a[i]; a[i]=a[j]; a[j]=t; } while (0)
#define ASORT_EXTRA_ARGS , struct elt *a
#include <ucw/sorter/array-simple.h>

#define ASORT_PREFIX(x) asi_##x
#define ASORT_KEY_TYPE u32
#define ASORT_ELT(i) ind[i]->key
#define ASORT_SWAP(i,j) do { struct elt *t=ind[i]; ind[i]=ind[j]; ind[j]=t; } while (0)
#include <ucw/sorter/array-simple.h>

static void r1_sort(void)
{
  struct elt *from = ary, *to = alt, *tmp;
#define BITS 8
  uns cnt[1 << BITS];
  for (uns sh=0; sh<32; sh+=BITS)
    {
      bzero(cnt, sizeof(cnt));
      for (uns i=0; i<n; i++)
	cnt[(from[i].key >> sh) & ((1 << BITS) - 1)]++;
      uns pos = 0;
      for (uns i=0; i<(1<<BITS); i++)
	{
	  uns c = cnt[i];
	  cnt[i] = pos;
	  pos += c;
	}
      ASSERT(pos == n);
      for (uns i=0; i<n; i++)
	to[cnt[(from[i].key >> sh) & ((1 << BITS) - 1)]++] = from[i];
      ASSERT(cnt[(1 << BITS)-1] == n);
      tmp=from, from=to, to=tmp;
    }
  ary = from;
#undef BITS
}

static void r1b_sort(void)
{
  struct elt *from = ary, *to = alt, *tmp;
#define BITS 8
  uns cnt[1 << BITS], cnt2[1 << BITS];
  for (uns sh=0; sh<32; sh+=BITS)
    {
      if (sh)
	memcpy(cnt, cnt2, sizeof(cnt));
      else
	{
	  bzero(cnt, sizeof(cnt));
	  for (uns i=0; i<n; i++)
	    cnt[(from[i].key >> sh) & ((1 << BITS) - 1)]++;
	}
      uns pos = 0;
      for (uns i=0; i<(1<<BITS); i++)
	{
	  uns c = cnt[i];
	  cnt[i] = pos;
	  pos += c;
	}
      ASSERT(pos == n);
      bzero(cnt2, sizeof(cnt2));
      for (uns i=0; i<n; i++)
	{
	  cnt2[(from[i].key >> (sh + BITS)) & ((1 << BITS) - 1)]++;
	  to[cnt[(from[i].key >> sh) & ((1 << BITS) - 1)]++] = from[i];
	}
      ASSERT(cnt[(1 << BITS)-1] == n);
      tmp=from, from=to, to=tmp;
    }
  ary = from;
#undef BITS
}

static void r1c_sort(void)
{
  uns cnt[256];
  struct elt *ptrs[256], *x, *lim;

  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    cnt[x++->key & 255]++;

#define PTRS(start) x=start; for (uns i=0; i<256; i++) { ptrs[i]=x; x+=cnt[i]; }

  PTRS(alt);
  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 8) & 255]++;
      *ptrs[x->key & 255]++ = *x;
      x++;
    }

  PTRS(ary);
  x = alt; lim = alt + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 16) & 255]++;
      *ptrs[(x->key >> 8) & 255]++ = *x;
      x++;
    }

  PTRS(alt);
  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 24) & 255]++;
      *ptrs[(x->key >> 16) & 255]++ = *x;
      x++;
    }

  PTRS(ary);
  x = alt; lim = alt + n;
  while (x < lim)
    {
      *ptrs[(x->key >> 24) & 255]++ = *x;
      x++;
    }
#undef PTRS
}

#include <emmintrin.h>

static inline void sse_copy_elt(struct elt *to, struct elt *from)
{
  __m128i m = _mm_load_si128((__m128i *) from);
  _mm_store_si128((__m128i *) to, m);
}

static void r1c_sse_sort(void)
{
  uns cnt[256];
  struct elt *ptrs[256], *x, *lim;

  ASSERT(sizeof(struct elt) == 16);
  ASSERT(!((uintptr_t)alt & 15));
  ASSERT(!((uintptr_t)ary & 15));

  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    cnt[x++->key & 255]++;

#define PTRS(start) x=start; for (uns i=0; i<256; i++) { ptrs[i]=x; x+=cnt[i]; }

  PTRS(alt);
  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 8) & 255]++;
      sse_copy_elt(ptrs[x->key & 255]++, x);
      x++;
    }

  PTRS(ary);
  x = alt; lim = alt + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 16) & 255]++;
      sse_copy_elt(ptrs[(x->key >> 8) & 255]++, x);
      x++;
    }

  PTRS(alt);
  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 24) & 255]++;
      sse_copy_elt(ptrs[(x->key >> 16) & 255]++, x);
      x++;
    }

  PTRS(ary);
  x = alt; lim = alt + n;
  while (x < lim)
    {
      sse_copy_elt(ptrs[(x->key >> 24) & 255]++, x);
      x++;
    }
#undef PTRS
}

static void r1d_sort(void)
{
  uns cnt[256];
  struct elt *ptrs[256], *x, *y, *lim;

  ASSERT(!(n % 4));

  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[x++->key & 255]++;
      cnt[x++->key & 255]++;
      cnt[x++->key & 255]++;
      cnt[x++->key & 255]++;
    }

#define PTRS(start) x=start; for (uns i=0; i<256; i++) { ptrs[i]=x; x+=cnt[i]; }

  PTRS(alt);
  x = ary; y = ary+n/2; lim = ary + n/2;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 8) & 255]++;
      cnt[(y->key >> 8) & 255]++;
      *ptrs[x->key & 255]++ = *x;
      *ptrs[y->key & 255]++ = *y;
      x++, y++;
      cnt[(x->key >> 8) & 255]++;
      cnt[(y->key >> 8) & 255]++;
      *ptrs[x->key & 255]++ = *x;
      *ptrs[y->key & 255]++ = *y;
      x++, y++;
    }

  PTRS(ary);
  x = alt; lim = alt + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 16) & 255]++;
      *ptrs[(x->key >> 8) & 255]++ = *x;
      x++;
      cnt[(x->key >> 16) & 255]++;
      *ptrs[(x->key >> 8) & 255]++ = *x;
      x++;
    }

  PTRS(alt);
  x = ary; lim = ary + n;
  bzero(cnt, sizeof(cnt));
  while (x < lim)
    {
      cnt[(x->key >> 24) & 255]++;
      *ptrs[(x->key >> 16) & 255]++ = *x;
      x++;
      cnt[(x->key >> 24) & 255]++;
      *ptrs[(x->key >> 16) & 255]++ = *x;
      x++;
    }

  PTRS(ary);
  x = alt; lim = alt + n;
  while (x < lim)
    {
      *ptrs[(x->key >> 24) & 255]++ = *x;
      x++;
      *ptrs[(x->key >> 24) & 255]++ = *x;
      x++;
    }
#undef PTRS
}

static void r2_sort(void)
{
  struct elt *from = ary, *to = alt;
#define BITS 14
  uns cnt[1 << BITS];
  bzero(cnt, sizeof(cnt));
  for (uns i=0; i<n; i++)
    cnt[(from[i].key >> (32 - BITS)) & ((1 << BITS) - 1)]++;
  uns pos = 0;
  for (uns i=0; i<(1<<BITS); i++)
    {
      uns c = cnt[i];
      cnt[i] = pos;
      pos += c;
    }
  ASSERT(pos == n);
  for (uns i=0; i<n; i++)
    to[cnt[(from[i].key >> (32 - BITS)) & ((1 << BITS) - 1)]++] = from[i];
  ASSERT(cnt[(1 << BITS)-1] == n);

  pos = 0;
  for (uns i=0; i<(1 << BITS); i++)
    {
      as_sort(cnt[i] - pos, alt+pos);
      pos = cnt[i];
    }
  ary = alt;
#undef BITS
}

static void r3_sort(void)
{
#define BITS 10
#define LEVELS 2
#define BUCKS (1 << BITS)
#define THRESHOLD 5000
#define ODDEVEN 0

  auto void r3(struct elt *from, struct elt *to, uns n, uns lev);
  void r3(struct elt *from, struct elt *to, uns n, uns lev)
  {
    uns sh = 32 - lev*BITS;
    uns cnt[BUCKS];
    bzero(cnt, sizeof(cnt));
    for (uns i=0; i<n; i++)
      cnt[(from[i].key >> sh) & (BUCKS - 1)]++;
    uns pos = 0;
    for (uns i=0; i<BUCKS; i++)
      {
	uns c = cnt[i];
	cnt[i] = pos;
	pos += c;
      }
    ASSERT(pos == n);
    for (uns i=0; i<n; i++)
#if 1
      to[cnt[(from[i].key >> sh) & (BUCKS - 1)]++] = from[i];
#else
      sse_copy_elt(&to[cnt[(from[i].key >> sh) & (BUCKS - 1)]++], &from[i]);
#endif
    pos = 0;
    for (uns i=0; i<BUCKS; i++)
      {
	uns l = cnt[i]-pos;
	if (lev >= LEVELS || l <= THRESHOLD)
	  {
	    as_sort(l, to+pos);
	    if ((lev % 2) != ODDEVEN)
	      memcpy(from+pos, to+pos, l * sizeof(struct elt));
	  }
	else
	  r3(to+pos, from+pos, l, lev+1);
	pos = cnt[i];
      }
  }

  r3(ary, alt, n, 1);
  if (ODDEVEN)
    ary = alt;

#undef ODDEVEN
#undef THRESHOLD
#undef BUCKS
#undef LEVELS
#undef BITS
}

static inline struct elt *mrg(struct elt *x, struct elt *xl, struct elt *y, struct elt *yl, struct elt *z)
{
  for (;;)
    {
      if (x->key <= y->key)
	{
	  *z++ = *x++;
	  if (x >= xl)
	    goto xend;
	}
      else
	{
	  *z++ = *y++;
	  if (y >= yl)
	    goto yend;
	}
    }

 xend:
  while (y < yl)
    *z++ = *y++;
  return z;

 yend:
  while (x < xl)
    *z++ = *x++;
  return z;
}

static void mergesort(void)
{
  struct elt *from, *to;
  uns lev = 0;
  if (1)
    {
      struct elt *x = ary, *z = alt, *last = ary + (n & ~1U);
      while (x < last)
	{
	  if (x[0].key < x[1].key)
	    *z++ = *x++, *z++ = *x++;
	  else
	    {
	      *z++ = x[1];
	      *z++ = x[0];
	      x += 2;
	    }
	}
      if (n % 2)
	*z = *x;
      lev++;
    }
  for (; (1U << lev) < n; lev++)
    {
      if (lev % 2)
	from = alt, to = ary;
      else
	from = ary, to = alt;
      struct elt *x, *z, *last;
      x = from;
      z = to;
      last = from + n;
      uns step = 1 << lev;
      while (x + 2*step <= last)
	{
	  z = mrg(x, x+step, x+step, x+2*step, z);
	  x += 2*step;
	}
      if (x + step < last)
	mrg(x, x+step, x+step, last, z);
      else
	memcpy(z, x, (byte*)last - (byte*)x);
    }
  if (lev % 2)
    ary = alt;
}

static void sampsort(uns n, struct elt *ar, struct elt *al, struct elt *dest, byte *wbuf)
{
#define WAYS 256
  struct elt k[WAYS];
  uns cnt[WAYS];
  bzero(cnt, sizeof(cnt));
  for (uns i=0; i<WAYS; i++)
    k[i] = ar[random() % n];
  as_sort(WAYS, k);
  for (uns i=0; i<n; i++)
    {
      uns w = 0;
#define FW(delta) if (ar[i].key > k[w+delta].key) w += delta
      FW(128);
      FW(64);
      FW(32);
      FW(16);
      FW(8);
      FW(4);
      FW(2);
      FW(1);
      wbuf[i] = w;
      cnt[w]++;
    }
  struct elt *y = al, *way[WAYS], *z;
  for (uns i=0; i<WAYS; i++)
    {
      way[i] = y;
      y += cnt[i];
    }
  ASSERT(y == al+n);
  for (uns i=0; i<n; i++)
    {
      uns w = wbuf[i];
      *way[w]++ = ar[i];
    }
  y = al;
  z = ar;
  for (uns i=0; i<WAYS; i++)
    {
      if (cnt[i] >= 1000)
	sampsort(cnt[i], y, z, dest, wbuf);
      else
	{
	  as_sort(cnt[i], y);
	  if (al != dest)
	    memcpy(z, y, cnt[i]*sizeof(struct elt));
	}
      y += cnt[i];
      z += cnt[i];
    }
#undef FW
#undef WAYS
}

static void samplesort(void)
{
  byte *aux = xmalloc(n);
  sampsort(n, ary, alt, ary, aux);
  xfree(aux);
}

static void sampsort2(uns n, struct elt *ar, struct elt *al, struct elt *dest, byte *wbuf)
{
#define WAYS 256
  struct elt k[WAYS];
  uns cnt[WAYS];
  bzero(cnt, sizeof(cnt));
  for (uns i=0; i<WAYS; i++)
    k[i] = ar[random() % n];
  as_sort(WAYS, k);
  struct elt *k1 = ar, *k2 = ar+1, *kend = ar+n;
  byte *ww = wbuf;
  while (k2 < kend)
    {
      uns w1 = 0, w2 = 0;
#define FW1(delta) if (k1->key > k[w1+delta].key) w1 += delta
#define FW2(delta) if (k2->key > k[w2+delta].key) w2 += delta
      FW1(128); FW2(128);
      FW1(64);  FW2(64);
      FW1(32);  FW2(32);
      FW1(16);  FW2(16);
      FW1(8);   FW2(8);
      FW1(4);   FW2(4);
      FW1(2);   FW2(2);
      FW1(1);   FW2(1);
      *ww++ = w1;
      *ww++ = w2;
      cnt[w1]++;
      cnt[w2]++;
      k1 += 2;
      k2 += 2;
    }
  if (k1 < kend)
    {
      uns w1 = 0;
      FW1(128); FW1(64); FW1(32); FW1(16);
      FW1(8); FW1(4); FW1(2); FW1(1);
      *ww++ = w1;
      cnt[w1]++;
    }
  struct elt *y = al, *way[WAYS], *z;
  for (uns i=0; i<WAYS; i++)
    {
      way[i] = y;
      y += cnt[i];
    }
  ASSERT(y == al+n);
  for (uns i=0; i<n; i++)
    {
      uns w = wbuf[i];
      *way[w]++ = ar[i];
    }
  y = al;
  z = ar;
  for (uns i=0; i<WAYS; i++)
    {
      if (cnt[i] >= 1000)
	sampsort2(cnt[i], y, z, dest, wbuf);
      else
	{
	  as_sort(cnt[i], y);
	  if (al != dest)
	    memcpy(z, y, cnt[i]*sizeof(struct elt));
	}
      y += cnt[i];
      z += cnt[i];
    }
#undef FW1
#undef FW2
#undef WAYS
}

static void samplesort2(void)
{
  byte *aux = xmalloc(n);
  sampsort2(n, ary, alt, ary, aux);
  xfree(aux);
}

static void heapsort(void)
{
#define H_LESS(_a,_b) ((_a).key > (_b).key)
  struct elt *heap = ary-1;
  HEAP_INIT(struct elt, heap, n, H_LESS, HEAP_SWAP);
  uns nn = n;
  while (nn)
    HEAP_DELMIN(struct elt, heap, nn, H_LESS, HEAP_SWAP);
#undef H_LESS
}

static void heapsort_ind(void)
{
#define H_LESS(_a,_b) ((_a)->key > (_b)->key)
  struct elt **heap = ind-1;
  HEAP_INIT(struct elt *, heap, n, H_LESS, HEAP_SWAP);
  uns nn = n;
  while (nn)
    HEAP_DELMIN(struct elt *, heap, nn, H_LESS, HEAP_SWAP);
#undef H_LESS
}

static void mk_ary(void)
{
  ary = array0;
  alt = array1;
  md5_context ctx;
  md5_init(&ctx);
  u32 block[16];
  bzero(block, sizeof(block));

  sum = 0;
  for (uns i=0; i<n; i++)
    {
#if 1
      if (!(i % 4))
	{
	  block[i%16] = i;
	  md5_transform(ctx.buf, block);
	}
      ary[i].key = ctx.buf[i%4];
#else
      ary[i].key = i*(~0U/(n-1));
#endif
      for (uns j=1; j<sizeof(struct elt)/4; j++)
	((u32*)&ary[i])[j] = ROL(ary[i].key, 3*j);
      sum ^= ary[i].key;
    }
}

static void chk_ary(void)
{
  u32 s = ary[0].key;
  for (uns i=1; i<n; i++)
    if (ary[i].key < ary[i-1].key)
      die("Missorted at %d", i);
    else
      s ^= ary[i].key;
  if (s != sum)
    die("Corrupted");
}

static void mk_ind(void)
{
  mk_ary();
  ind = xmalloc(sizeof(struct elt *) * n);
  for (uns i=0; i<n; i++)
    ind[i] = &ary[i];
}

static void chk_ind(void)
{
  u32 s = ind[0]->key;
  for (uns i=1; i<n; i++)
    if (ind[i]->key < ind[i-1]->key)
      die("Missorted at %d", i);
    else
      s ^= ind[i]->key;
  if (s != sum)
    die("Corrupted");
  xfree(ind);
}

int main(int argc, char **argv)
{
  log_init(argv[0]);

  int opt;
  uns op = 0;
  while ((opt = cf_getopt(argc, argv, CF_SHORT_OPTS "1", CF_NO_LONG_OPTS, NULL)) >= 0)
    switch (opt)
      {
      case '1':
	op |= (1 << (opt - '0'));
	break;
      default:
	die("usage?");
      }

  array0 = alloc_elts(n);
  array1 = alloc_elts(n);
  for (uns i=0; i<n; i++)
    array0[i] = array1[i] = (struct elt) { 0 };

  msg(L_INFO, "Testing with %u elements", n);

  mk_ary();
  timestamp_t timer;
  init_timer(&timer);
  for (uns i=0; i<5; i++)
    {
#if 1
      memcpy(alt, ary, sizeof(struct elt) * n);
      memcpy(ary, alt, sizeof(struct elt) * n);
#else
      for (uns j=0; j<n; j++)
	alt[j] = ary[j];
      for (uns j=0; j<n; j++)
	ary[j] = alt[j];
#endif
    }
  msg(L_DEBUG, "memcpy: %d", get_timer(&timer)/10);

#define BENCH(type, name, func) mk_##type(); init_timer(&timer); func; msg(L_DEBUG, name ": %d", get_timer(&timer)); chk_##type()

  BENCH(ary, "qsort", qsort(ary, n, sizeof(struct elt), comp));
  BENCH(ary, "arraysort", as_sort(n, ary));
  BENCH(ind, "indirect qsort", qsort(ind, n, sizeof(struct elt *), comp_ind));
  BENCH(ind, "indirect arraysort", asi_sort(n));
  BENCH(ary, "radix1", r1_sort());
  BENCH(ary, "radix1b", r1b_sort());
  BENCH(ary, "radix1c", r1c_sort());
  BENCH(ary, "radix1c-sse", r1c_sse_sort());
  BENCH(ary, "radix1d", r1d_sort());
  BENCH(ary, "radix2", r2_sort());
  BENCH(ary, "radix3", r3_sort());
  BENCH(ary, "mergesort", mergesort());
  BENCH(ary, "samplesort", samplesort());
  BENCH(ary, "samplesort2", samplesort2());
  BENCH(ary, "heapsort", heapsort());
  BENCH(ind, "indirect heapsort", heapsort_ind());

  free_elts(array0, n);
  free_elts(array1, n);
  return 0;
}
