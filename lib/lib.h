/*
 *	Sherlock Library -- Miscellaneous Functions
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <lib/config.h>

/* Temporary Files */

#define TMP_DIR "tmp"
#define TMP_DIR_LEN 3

struct tempfile {
  int fh;
  byte name[32];
};

void open_temp(struct tempfile *, byte *);
void delete_temp(struct tempfile *);
ulg temprand(uns);

#define TF_GENERIC "t"
#define TF_QUEUE_CONTROL "c"
#define TF_QUEUE_DATA "d"
#define TF_DECODE "x"

/* Config Files */

struct cfitem {
  byte *name;
  int type;
  void *var;
};

#define CI_STOP 0
#define CI_INT 1
#define CI_STRING 2
#define CI_FUNCTION 3

typedef byte *(*ci_func)(struct cfitem *, byte *);

void cf_read(byte *, struct cfitem *);

/* Logging */

#define L_DEBUG "<0>"
#define L_INFO "<2>"
#define L_WARN "<4>"
#define L_ERROR "<6>"
#define L_FATAL "<9>"

void log(byte *, ...);
void die(byte *, ...) NONRET;
void initlog(byte *);

/* Allocation */

void *xmalloc(uns);
byte *stralloc(byte *);

/* Content-Type pattern matching and filters */

struct ct_filter;

int match_ct_patt(byte *, byte *);

struct ct_filter *new_ct_filter(void);
byte *add_ct_filter(struct ct_filter *, byte *);
int match_ct_filter(struct ct_filter *, byte *);

/* Binary log */

#ifdef HAVE_FFS
#define log2(x) (ffs(x) - 1)
#else
int log2(ulg);
#endif

/* obj.c */

struct odes {				/* Object description */
  struct oattr *attrs;
};

struct oattr {				/* Object attribute */
  struct oattr *next, *same;
  byte attr;
  byte val[1];
};

void obj_dump(struct odes *);
struct odes *obj_fload(FILE *);
struct odes *obj_new(void);
struct odes *obj_load(byte *);
void obj_fwrite(FILE *, struct odes *);	/* Closes the file afterwards... */
void obj_free(struct odes *);
struct oattr *find_attr(struct odes *, uns);
struct oattr *find_attr_last(struct odes *, uns);
byte *find_aval(struct odes *, uns);
struct oattr *set_attr(struct odes *, uns, byte *);
struct oattr *set_attr_num(struct odes *, uns, uns);
struct oattr *add_attr(struct odes *, struct oattr *, uns, byte *);
struct oattr *prepend_attr(struct odes *, uns, byte *);

/* oname.c */

void mk_obj_name(byte *, ulg, byte *);
FILE *create_obj_file(byte *, ulg);

/* wordsplit.c */

int wordsplit(byte *, byte **, uns);

/* patmatch.c */

int match_pattern(byte *, byte *);
