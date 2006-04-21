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

enum cf_class {
  CC_END,				// end of list
  CC_STATIC,				// single variable or static array
  CC_DYNAMIC,				// dynamically allocated array
  CC_PARSER,				// arbitrary parser function
  CC_SECTION,				// section appears exactly once
  CC_LIST				// list with 0..many nodes
};

enum cf_type {
  CT_INT, CT_U64, CT_DOUBLE,		// number types
  CT_IP,				// IP address
  CT_STRING				// string type
};

typedef byte *cf_parser(uns number, byte **pars, void *ptr);
  /* A parser function gets an array of (strdup'ed) strings and a pointer with
   * the customized information (most likely the target address).  It can store
   * the parsed value anywhere in any way it likes, however it must first call
   * cf_journal_block() on the overwritten memory block.  It returns an error
   * message or NULL if everything is all right.  */
typedef byte *cf_hook(void *ptr);
  /* An init- or commit-hook gets a pointer to the section or NULL if this
   * is the global section.  It returns an error message or NULL if everything
   * is all right.  The init-hook should fill in default values (needed for
   * dynamically allocated nodes of link lists or for filling global variables
   * that are run-time dependent).  The commit-hook should perform sanity
   * checks and postprocess the parsed values.  Commit-hooks must call
   * cf_journal_block() too.  */

struct cf_section;
struct cf_item {
  enum cf_class cls;
  byte *name;
  int number;				// length of an array or #parameters of a parser (negative means at most)
  void *ptr;				// pointer to a global variable or an offset in a section
  union {
    enum cf_type type;			// type of a static or dynamic attribute
    struct cf_section *sec;		// declaration of a section or a list
    cf_parser *par;			// parser function
  } u;
};

struct cf_section {
  uns size;				// 0 for a global block, sizeof(struct) for a section
  cf_hook *init;			// fills in default values (otherwise 0's are used)
  cf_hook *commit;			// verifies parsed data (optional)
  struct cf_item *cfg;			// CC_END-terminated array of items
  uns flags;				// for internal use only
};

/* Declaration of cf_section */
#define CF_TYPE(s)	.size = sizeof(s)
#define CF_INIT(f)	.init = (cf_hook*) f
#define CF_COMMIT(f)	.commit = (cf_hook*) f
#define CF_ITEMS	.cfg = ( struct cf_item[] )
#define CF_END		{ .cls = CC_END }
/* Configuration items */
struct clist;
#define CF_STATIC(n,p,T,t,c)	{ .cls = CC_STATIC, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,t*), .u.type = CT_##T }
#define CF_DYNAMIC(n,p,T,t,c)	{ .cls = CC_DYNAMIC, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,t**), .u.type = CT_##T }
#define CF_PARSER(n,p,f,c)	{ .cls = CC_PARSER, .name = n, .number = c, .ptr = p, .u.par = (cf_parser*) f }
#define CF_SECTION(n,p,s)	{ .cls = CC_SECTION, .name = n, .number = 1, .ptr = p, .u.sec = s }
#define CF_LIST(n,p,s)		{ .cls = CC_LIST, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,struct clist*), .u.sec = s }
/* Configuration items for basic types */
#define CF_INT(n,p)		CF_STATIC(n,p,INT,int,1)
#define CF_INT_ARY(n,p,c)	CF_STATIC(n,p,INT,int,c)
#define CF_INT_DYN(n,p,c)	CF_DYNAMIC(n,p,INT,int,c)
#define CF_UNS(n,p)		CF_STATIC(n,p,INT,uns,1)
#define CF_UNS_ARY(n,p,c)	CF_STATIC(n,p,INT,uns,c)
#define CF_UNS_DYN(n,p,c)	CF_DYNAMIC(n,p,INT,uns,c)
#define CF_U64(n,p)		CF_STATIC(n,p,U64,u64,1)
#define CF_U64_ARY(n,p,c)	CF_STATIC(n,p,U64,u64,c)
#define CF_U64_DYN(n,p,c)	CF_DYNAMIC(n,p,U64,u64,c)
#define CF_DOUBLE(n,p)		CF_STATIC(n,p,DOUBLE,double,1)
#define CF_DOUBLE_ARY(n,p,c)	CF_STATIC(n,p,DOUBLE,double,c)
#define CF_DOUBLE_DYN(n,p,c)	CF_DYNAMIC(n,p,DOUBLE,double,c)
#define CF_IP(n,p)		CF_STATIC(n,p,IP,u32,1)
#define CF_IP_ARY(n,p,c)	CF_STATIC(n,p,IP,u32,c)
#define CF_IP_DYN(n,p,c)	CF_DYNAMIC(n,p,IP,u32,c)
#define CF_STRING(n,p)		CF_STATIC(n,p,STRING,byte*,1)
#define CF_STRING_ARY(n,p,c)	CF_STATIC(n,p,STRING,byte*,c)
#define CF_STRING_DYN(n,p,c)	CF_DYNAMIC(n,p,STRING,byte*,c)

#define DYN_LEN(a) *(uns*)(a-1)
  // length of a dynamic array
#define DYN_ALLOC(type,len,val...) (type[]) { (type)len, ##val } + 1
  // creates a static instance of a dynamic array

/* Memory allocation */
struct mempool;
extern struct mempool *cf_pool;
void *cf_malloc(uns size);
void *cf_malloc_zero(uns size);
byte *cf_strdup(byte *s);
byte *cf_printf(char *fmt, ...);

/* Undo journal for error recovery */
extern uns cf_need_journal;
void cf_journal_block(void *ptr, uns len);

/* Declaration */
void cf_declare_section(byte *name, struct cf_section *sec);
void cf_init_section(byte *name, struct cf_section *sec, void *ptr);

/* Safe reloading and loading of configuration files */
extern byte *cf_def_file;
byte *cf_reload(byte *file);
byte *cf_load(byte *file);
byte *cf_set(byte *string);

/* Parsers for basic types */
byte *cf_parse_int(byte *str, int *ptr);
byte *cf_parse_u64(byte *str, u64 *ptr);
byte *cf_parse_double(byte *str, double *ptr);
byte *cf_parse_ip(byte *p, u32 *varp);

#endif
