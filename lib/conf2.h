/*
 *	UCW Library -- Reading of configuration files
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_UCW_CONF2_H
#define	_UCW_CONF2_H

enum cf_type {
  CT_END,				// end of list
  CT_INT, CT_U64, CT_DOUBLE,		// number types
  CT_STRING,				// string type
  CT_PARSER,				// arbitrary parser function
  CT_SECTION,				// section appears exactly once
  CT_LIST				// list with 0..many nodes
};

typedef byte *cf_hook(void *ptr);
  /* An init- or commit-hook gets a pointer to the section or NULL if this
   * is the global section.  It returns an error message or NULL if everything
   * is all right.  */
typedef byte *cf_parser(uns number, byte **pars, void *ptr);
  /* A parser function an array of strings and stores the parsed value in any
   * way it likes into *ptr.  It returns an error message or NULL if everything
   * is all right.  */

struct cf_section;
struct cf_item {
  enum cf_type type;
  byte *name;
  int number;				// number of values: k>=0 means exactly k, k<0 means at most -k
  void *ptr;				// pointer to a global variable or an offset in a section
  union {
    struct cf_section *sub;		// declaration of a section or a list
    cf_parser *par;			// parser function
  } ptr2;
};

struct cf_section {
  uns size;				// 0 for a global block, sizeof(struct) for a section
  cf_hook *init;			// fills in default values
  cf_hook *commit;			// verifies parsed data and checks ranges (optional)
  struct cf_item *cfg;			// CT_END-terminated array of items
};

/* Declaration of cf_section */
#define CF_TYPE(s)	.size = sizeof(s),
#define CF_INIT(f)	.init = (cf_hook*) f,
#define CF_COMMIT(f)	.commit = (cf_hook*) f,
#define CF_ITEMS(i)	.cfg = ( struct cf_item[] ) { i { .type = CT_END } },
/* Configuration items for single variables */
#define CF_INT(n,p)	{ .type = CT_INT, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,int*) },
#define CF_U64(n,p)	{ .type = CT_U64, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,u64*) },
#define CF_DOUBLE(n,p)	{ .type = CT_DOUBLE, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,double*) },
#define CF_STRING(n,p)	{ .type = CT_STRING, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,byte**) },
/* Configuration items for arrays of variables */
#define CF_INT_ARY(n,p,c)	{ .type = CT_INT, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,int**) },
#define CF_U64_ARY(n,p,c)	{ .type = CT_U64, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,u64**) },
#define CF_DOUBLE_ARY(n,p,c)	{ .type = CT_DOUBLE, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,double**) },
#define CF_STRING_ARY(n,p,c)	{ .type = CT_STRING, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,byte***) },

#define ARRAY_ALLOC(type,len,val...) (type[]) { (type)len, ##val } + 1
  // creates an array with an allocated space in the front for the (Pascal-like) length
#define ARRAY_LEN(a) *(uns*)(a-1)
  // length of the array

/* Configuration items for sections, lists, and parsed items */
struct clist;
#define CF_PARSER(n,p,f,c)	{ .type = CT_PARSER, .name = n, .number = c, .ptr = p, .ptr2.par = (cf_parser*) f },
#define CF_SECTION(n,p,s)	{ .type = CT_SECTION, .name = n, .number = 1, .ptr = p, .ptr2.sub = s },
#define CF_LIST(n,p,s)		{ .type = CT_LIST, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,struct clist*), .ptr2.sub = s },

/* Memory allocation */
void *cf_malloc(uns size);
void *cf_malloc_zero(uns size);
byte *cf_strdup(byte *s);
byte *cf_printf(char *fmt, ...);

/* Undo journal for error recovery */
uns cf_journal_active(uns flag);
void cf_journal_block(void *ptr, uns len);

#endif
