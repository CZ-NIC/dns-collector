/*
 *	Sherlock Library -- Miscellaneous Functions
 *
 *	(c) 1997--2000 Martin Mares <mj@ucw.cz>
 */

/*
 *  This file should be included as the very first include in all
 *  source files, especially before all OS includes since it sets
 *  up libc feature macros.
 */

#ifndef _SHERLOCK_LIB_H
#define _SHERLOCK_LIB_H

#include "lib/config.h"

/* Tell libc we're going to use all extensions available */

#define _GNU_SOURCE

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Ugly structure handling macros */

#define OFFSETOF(s, i) ((unsigned int)&((s *)0)->i)
#define SKIP_BACK(s, i, p) ((s *)((char *)p - OFFSETOF(s, i)))
#define ALIGN(s, a) (((s)+a-1)&~(a-1))

/* Temporary Files */

#define TMP_DIR "tmp"
#define TMP_DIR_LEN 3

struct tempfile {
  int fh;
  byte name[32];
};

void open_temp(struct tempfile *, byte *);
void delete_temp(struct tempfile *);
u32 temprand(uns);

/* FIXME: Remove? */
#define TF_GENERIC "t"
#define TF_QUEUE_CONTROL "c"
#define TF_QUEUE_DATA "d"
#define TF_DECODE "x"
#define TF_TRANSFORM "s"
#define TF_OBJECT "o"

/* Logging */

/* FIXME: Define new logging mechanism? */

#define L_DEBUG "<0>"
#define L_INFO "<2>"
#define L_WARN "<4>"
#define L_ERROR "<6>"
#define L_FATAL "<9>"

void log(byte *, ...);
void die(byte *, ...) NONRET;
void initlog(byte *);
void open_log_file(byte *);

#ifdef DEBUG
#define ASSERT(x) do { if (!(x)) die("Assertion `%s' failed at %s:%d", #x, __FILE__, __LINE__); } while(0)
#else
#define ASSERT(x) do { } while(0)
#endif

/* Allocation */

#ifdef DMALLOC
/*
 * The standard dmalloc macros tend to produce lots of namespace
 * conflicts and we use only xmalloc and xfree, so we can define
 * the stubs ourselves.
 */
#define DMALLOC_DISABLE
#include <dmalloc.h>
#define xmalloc(size) _xmalloc_leap(__FILE__, __LINE__, size)
#define xrealloc(ptr,size) _xrealloc_leap(__FILE__, __LINE__, ptr, size)
#define xfree(ptr) _xfree_leap(__FILE__, __LINE__, ptr)
#else
/*
 * Unfortunately, several libraries we might want to link to define
 * their own xmalloc and we don't want to interfere with them, hence
 * the renaming.
 */
#define xmalloc bird_xmalloc
void *xmalloc(unsigned);
void *xrealloc(void *, unsigned);
#define xfree(x) free(x)
#endif

byte *stralloc(byte *);

/* Content-Type pattern matching and filters */

int match_ct_patt(byte *, byte *);

/* Binary log */

int log2(u32);

/* wordsplit.c */

int wordsplit(byte *, byte **, uns);

/* pat(i)match.c */

int match_pattern(byte *, byte *);
int match_pattern_nocase(byte *, byte *);

/* md5hex.c */

void md5_to_hex(byte *, byte *);
void hex_to_md5(byte *, byte *);

#define MD5_SIZE 16
#define MD5_HEX_SIZE 33

/* prime.c */

int isprime(uns);
uns nextprime(uns);

/* timer.c */

void init_timer(void);
uns get_timer(void);

/* regex.c */

typedef struct regex regex;

regex *rx_compile(byte *r);
void rx_free(regex *r);
int rx_match(regex *r, byte *s);
int rx_subst(regex *r, byte *by, byte *src, byte *dest, uns destlen);

/* random.c */

uns random_max(uns);

/* mmap.c */

void *mmap_file(byte *name, unsigned *len);

#endif
