/*
 *	Sherlock Library -- Miscellaneous Functions
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef _SHERLOCK_LIB_H
#define _SHERLOCK_LIB_H

#include "lib/config.h"

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

void *xmalloc(uns);
void *xrealloc(void *, uns);
byte *stralloc(byte *);

/* Content-Type pattern matching and filters */

int match_ct_patt(byte *, byte *);

/* Binary log */

int log2(u32);

/* obj.c */

/* FIXME: What to do with this? */

struct odes {				/* Object description */
  struct oattr *attrs;
  struct mempool *pool;
};

struct oattr {				/* Object attribute */
  struct oattr *next, *same;
  byte attr;
  byte val[1];
};

void obj_dump(struct odes *);
struct odes *obj_fload(FILE *, byte *);
struct odes *obj_new(void);
struct odes *obj_load(byte *);
void obj_fwrite(FILE *, struct odes *);
void obj_write(byte *, struct odes *);
void obj_free(struct odes *);
struct oattr *find_attr(struct odes *, uns);
struct oattr *find_attr_last(struct odes *, uns);
uns del_attr(struct odes *, struct oattr *);
byte *find_aval(struct odes *, uns);
struct oattr *set_attr(struct odes *, uns, byte *);
struct oattr *set_attr_num(struct odes *, uns, uns);
struct oattr *add_attr(struct odes *, struct oattr *, uns, byte *);
struct oattr *prepend_attr(struct odes *, uns, byte *);

/* oname.c */

/* FIXME: Kill? */

#define OID_MIN 0x10000		/* Values less than this have special meaning */

oid_t new_oid(uns);
void mk_obj_name(byte *, oid_t, byte *);
int dump_obj_to_file(byte *, oid_t, struct odes *, int);

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
