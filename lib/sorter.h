/*
 *	Sherlock Library -- Universal Sorter
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

/*
 *  This is not a normal header file, it's a generator of sorting
 *  routines.  Each time you include it with parameters set in the
 *  corresponding preprocessor macros, it generates a file sorter
 *  with the parameters given.
 *
 *  Recognized parameter macros: (those marked with [*] are mandatory)
 *
 *  SORT_KEY	    [*]	data type capable of storing a single key
 *  SORT_PREFIX(x)  [*] add a name prefix (used on all global names
 *			defined by the sorter)
 *  SORT_PRESORT	include an in-core presorting pass
 *  SORT_UNIFY		merge items with identical keys
 *  SORT_DELETE_INPUT	a C expression, if true, the input files are
 *			deleted as soon as possible
 *  SORT_INPUT_FILE	input is a file with this name
 *  SORT_INPUT_FB	input is a fastbuf stream
 *  SORT_INPUT_FBPAIR	input is a pair of fastbuf streams
 *			(not supported by the presorter)
 *  SORT_OUTPUT_FILE	output is a file with this name
 *  SORT_OUTPUT_FB	output is a fastbuf stream
 *
 *  You also need to define some (usually inline) functions which
 *  are called by the sorter to process your data:
 *
 *  int PREFIX_compare(SORT_KEY *a, *b)
 *			compare two keys, result like strcmp
 *  int PREFIX_fetch_key(struct fastbuf *f, SORT_KEY *k)
 *			fetch next key, returns 1=ok, 0=eof
 *  void PREFIX_copy_data(struct fastbuf *src, *dest, SORT_KEY *k)
 *			write just fetched key k to dest and copy all data
 *			belonging to this key from src to dest.
 *  void PREFIX_merge_data(struct fastbuf *src1, *src2, *dest, SORT_KEY *k1, *k2)
 *			[used only in case SORT_UNIFY is defined]
 *			write just fetched key k to dest and merge data from
 *			two records with the same key (k1 and k2 are key occurences
 *			in the corresponding streams).
 *  char * PREFIX_fetch_item(struct fastbuf *f, SORT_KEY *k, char *limit)
 *			[used only with SORT_PRESORT]
 *			fetch data belonging to a just fetched key and store
 *			them to memory following the key, but not over limit.
 *			Returns a pointer to first byte after the data
 *			or NULL if the data don't fit.
 *			Important: keys carrying no data must be position
 *			independent.
 *  void PREFIX_store_item(struct fastbuf *f, SORT_KEY *k)
 *			[used only with SORT_PRESORT]
 *			write key and all its data read with PREFIX_fetch_data
 *			to the stream given.
 *  SORT_KEY * PREFIX_merge_items(SORT_KEY *a, SORT_KEY *b)
 *			[used only with SORT_PRESORT && SORT_UNIFY]
 *			merge two items with the same key, returns pointer
 *			to at most one of the items, the rest will be removed
 *			from the list of items, but not deallocated, so
 *			the remaining item can freely reference data of the
 *			other one.
 */

/* Declarations of externals from sorter.c */

#ifndef SORT_DECLS_READ
#define SORT_DECLS_READ

extern uns sorter_trace;
extern uns sorter_presort_bufsize;
extern uns sorter_stream_bufsize;

extern uns sorter_pass_counter, sorter_file_counter;
struct fastbuf *sorter_open_tmp(void);

#endif		/* !SORT_DECLS_READ */

/* The sorter proper */

#ifndef SORT_DECLARE_ONLY

#include "lib/fastbuf.h"
#include <unistd.h>
#include <fcntl.h>

#if !defined(SORT_KEY) || !defined(SORT_PREFIX)
#error Some of the mandatory configuration macros are missing.
#endif

#define P(x) SORT_PREFIX(x)
#define SWAP(x,y,z) do { z=x; x=y; y=z; } while(0)

#if defined(SORT_UNIFY) || defined(SORT_UNIQUE)
#define LESS <
#else
#define LESS <=
#endif

static void
P(pass)(struct fastbuf **fb1, struct fastbuf **fb2)
{
  struct fastbuf *in1 = *fb1;
  struct fastbuf *in2 = *fb2;
  struct fastbuf *out1 = NULL;
  struct fastbuf *out2 = NULL;
  SORT_KEY kbuf1, kbuf2, kbuf3, kbuf4;
  SORT_KEY *kin1 = &kbuf1;
  SORT_KEY *kprev1 = &kbuf2;
  SORT_KEY *kin2 = &kbuf3;
  SORT_KEY *kprev2 = &kbuf4;
  SORT_KEY *kout = NULL;
  SORT_KEY *ktmp;
  int next1, next2, comp;
  int run1, run2;
  uns run_count = 0;

  run1 = next1 = in1 ? P(fetch_key)(in1, kin1) : 0;
  run2 = next2 = in2 ? P(fetch_key)(in2, kin2) : 0;
  while (next1 || next2)
    {
      if (!run1)
	comp = 1;
      else if (!run2)
	comp = -1;
      else
	comp = P(compare)(kin1, kin2);
      ktmp = (comp <= 0) ? kin1 : kin2;
      if (!kout || !(P(compare)(kout, ktmp) LESS 0))
	{
	  struct fastbuf *t;
	  SWAP(out1, out2, t);
	  if (!out1)
	    out1 = sorter_open_tmp();
	  run_count++;
	}
      if (comp LESS 0)
	{
	  P(copy_data)(in1, out1, kin1);
	  SWAP(kin1, kprev1, ktmp);
	  next1 = P(fetch_key)(in1, kin1);
	  run1 = next1 && (P(compare)(kprev1, kin1) LESS 0);
	  kout = kprev1;
	}
#ifdef SORT_UNIFY
      else if (comp == 0)
	{
	  P(merge_data)(in1, in2, out1, kin1, kin2);
	  SWAP(kin1, kprev1, ktmp);
	  next1 = P(fetch_key)(in1, kin1); /* FIXME: Re-use other code? */
	  run1 = next1 && (P(compare)(kprev1, kin1) LESS 0);
	  SWAP(kin2, kprev2, ktmp);
	  next2 = P(fetch_key)(in2, kin2);
	  run2 = next2 && (P(compare)(kprev2, kin2) LESS 0);
	  kout = kprev2;
	}
#endif
      else
	{
	  P(copy_data)(in2, out1, kin2);
	  SWAP(kin2, kprev2, ktmp);
	  next2 = P(fetch_key)(in2, kin2);
	  run2 = next2 && (P(compare)(kprev2, kin2) LESS 0);
	  kout = kprev2;
	}
      if (!run1 && !run2)
	{
	  run1 = next1;
	  run2 = next2;
	}
    }
  bclose(in1);
  bclose(in2);
  if (sorter_trace)
    log(L_INFO, "Pass %d: %d runs, %d+%d KB", sorter_pass_counter, run_count,
	(out1 ? (int)((btell(out1) + 1023) / 1024) : 0),
	(out2 ? (int)((btell(out2) + 1023) / 1024) : 0));
  if (out1)				/* FIXME: What about empty output? */
    {
      bflush(out1);
      bsetpos(out1, 0);
    }
  if (out2)
    {
      bflush(out2);
      bsetpos(out2, 0);
    }
  *fb1 = out1;
  *fb2 = out2;
  sorter_pass_counter++;
}

static
#ifdef SORT_OUTPUT_FB
struct fastbuf *
#elif defined(SORT_OUTPUT_FILE)
void
#else
#error No output defined.
#endif
P(sort)(
#ifdef SORT_INPUT_FILE
byte *inname
#elif defined(SORT_INPUT_FB)
struct fastbuf *fb1
#elif defined(SORT_INPUT_FBPAIR)
struct fastbuf *fb1, struct fastbuf *fb2
#else
#error No input defined.
#endif
#ifdef SORT_OUTPUT_FILE
,byte *outname
#endif
)
{
#ifdef SORT_INPUT_FILE
  struct fastbuf *fb1, *fb2;
  fb1 = bopen(inname, O_RDONLY, sorter_stream_bufsize);
#ifdef SORT_DELETE_INPUT
  fb1->is_temp_file = SORT_DELETE_INPUT;
#endif
  fb2 = NULL;
#elif defined(SORT_INPUT_FB)
  struct fastbuf *fb2 = NULL;
#endif

  sorter_pass_counter = 1;
  do P(pass)(&fb1, &fb2); while (fb1 && fb2);
  if (!fb1)
    fb1 = fb2;
  fb1->is_temp_file = 0;

#ifdef SORT_OUTPUT_FB
  return fb1;
#else
  if (rename(fb1->name, outname) < 0)
    die("rename(%s,%s): %m", fb1->name, outname);
#endif
}

#undef P
#undef LESS
#undef SWAP

#endif		/* !SORT_DECLARE_ONLY */
