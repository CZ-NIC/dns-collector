/*
 *	Sherlock Library -- Miscellaneous Functions
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
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

/* Ugly structure handling macros */

#define OFFSETOF(s, i) ((unsigned int)&((s *)0)->i)
#define SKIP_BACK(s, i, p) ((s *)((char *)p - OFFSETOF(s, i)))
#define ALIGN(s, a) (((s)+a-1)&~(a-1))
#define UNALIGNED_PART(ptr, type) (((long) (ptr)) % sizeof(type))

/* Some other macros */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP(x,min,max) ({ int _t=x; (_t < min) ? min : (_t > max) ? max : _t; })
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

/* Logging */

#define L_DEBUG		'D'		/* Debugging messages */
#define L_INFO		'I'		/* Informational msgs, warnings and errors */
#define L_WARN		'W'
#define L_ERROR		'E'
#define L_INFO_R	'i'		/* Errors caused by external events */
#define L_WARN_R	'w'
#define L_ERROR_R	'e'
#define L_FATAL		'!'		/* die() */

void log(unsigned int cat, const char *msg, ...) __attribute__((format(printf,2,3)));
void die(byte *, ...) NONRET;
void log_init(byte *);
void log_file(byte *);
void log_fork(void);

#ifdef DEBUG
void assert_failed(char *assertion, char *file, int line) NONRET;
#define ASSERT(x) do { if (!(x)) assert_failed(#x, __FILE__, __LINE__); } while(0)
#else
void assert_failed(void) NONRET;
#define ASSERT(x) do { if (__builtin_constant_p(x) && !(x)) assert_failed(); } while(0)
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

void *xmalloc_zero(unsigned);
byte *stralloc(byte *);

/* Objects */

struct fastbuf;

struct odes {				/* Object description */
  struct oattr *attrs;
  struct mempool *pool;
  struct oattr *cached_attr;
};

struct oattr {				/* Object attribute */
  struct oattr *next, *same;
  byte attr;
  byte val[1];
};

void obj_dump(struct odes *);
struct odes *obj_new(struct mempool *);
int obj_read(struct fastbuf *, struct odes *);
void obj_write(struct fastbuf *, struct odes *);
void obj_write_nocheck(struct fastbuf *, struct odes *);
struct oattr *obj_find_attr(struct odes *, uns);
struct oattr *obj_find_attr_last(struct odes *, uns);
uns obj_del_attr(struct odes *, struct oattr *);
byte *obj_find_aval(struct odes *, uns);
struct oattr *obj_set_attr(struct odes *, uns, byte *);
struct oattr *obj_set_attr_num(struct odes *, uns, uns);
struct oattr *obj_add_attr(struct odes *, uns, byte *);
struct oattr *obj_prepend_attr(struct odes *, uns, byte *);
struct oattr *obj_insert_attr(struct odes *o, struct oattr *first, struct oattr *after, byte *v);

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

regex *rx_compile(byte *r, int icase);
void rx_free(regex *r);
int rx_match(regex *r, byte *s);
int rx_subst(regex *r, byte *by, byte *src, byte *dest, uns destlen);

/* random.c */

uns random_max(uns);

/* mmap.c */

void *mmap_file(byte *name, unsigned *len, int writeable);
void munmap_file(void *start, unsigned len);

/* proctitle.c */

void setproctitle_init(int argc, char **argv);
void setproctitle(char *msg, ...) __attribute__((format(printf,1,2)));

/* randomkey.c */

void randomkey(byte *buf, uns size);

#endif
