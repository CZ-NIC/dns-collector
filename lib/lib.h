/*
 *	Sherlock Library -- Miscellaneous Functions
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
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

/* Logging */

#define L_DEBUG		'D'		/* Debugging messages */
#define L_INFO		'I'		/* Informational msgs, warnings and errors */
#define L_WARN		'W'
#define L_ERROR		'E'
#define L_INFO_R	'i'		/* Errors caused by external events */
#define L_WARN_R	'w'
#define L_ERROR_R	'e'
#define L_FATAL		'!'		/* die() */

void log(unsigned int cat, byte *msg, ...);
void die(byte *, ...) NONRET;
void log_init(byte *);
void log_file(byte *);

#ifdef DEBUG
#define ASSERT(x) do { if (!(x)) die("Assertion `%s' failed at %s:%d", #x, __FILE__, __LINE__); } while(0)
#else
#define ASSERT(x) do { } while(0)
#endif

#ifdef LOCAL_DEBUG
#define DBG(x,y...) log(L_DEBUG, x,##y)
#else
#define DBG(x,y...) do { } while(0)
#endif

/* Memory allocation */

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
#define xmalloc sh_xmalloc
void *xmalloc(unsigned);
void *xrealloc(void *, unsigned);
#define xfree(x) free(x)
#endif

byte *stralloc(byte *);

/* Objects */

struct fastbuf;

struct odes {				/* Object description */
  struct oattr *attrs;
  struct mempool *pool, *local_pool;
};

struct oattr {				/* Object attribute */
  struct oattr *next, *same, *last_same;
  byte attr;
  byte val[1];
};

void obj_dump(struct odes *);
struct odes *obj_new(struct mempool *);
void obj_free(struct odes *);
int obj_read(struct fastbuf *, struct odes *);
void obj_write(struct fastbuf *, struct odes *);
struct oattr *obj_find_attr(struct odes *, uns);
struct oattr *obj_find_attr_last(struct odes *, uns);
uns obj_del_attr(struct odes *, struct oattr *);
byte *obj_find_aval(struct odes *, uns);
struct oattr *obj_set_attr(struct odes *, uns, byte *);
struct oattr *obj_set_attr_num(struct odes *, uns, uns);
struct oattr *obj_add_attr(struct odes *, struct oattr *, uns, byte *);

/* Content-Type pattern matching and filters */

int match_ct_patt(byte *, byte *);

/* log2.c */

int log2(u32);

/* wordsplit.c */

int wordsplit(byte *, byte **, uns);

/* pat(i)match.c: Matching of shell patterns */

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
