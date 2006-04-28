/*
 *	UCW Library -- Configuration files
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_UCW_CONF_H
#define	_UCW_CONF_H

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
  CT_STRING,				// string type
  CT_LOOKUP,				// in a string table
  CT_USER				// user-defined type
};

struct fastbuf;
typedef byte *cf_parser(uns number, byte **pars, void *ptr);
  /* A parser function gets an array of (strdup'ed) strings and a pointer with
   * the customized information (most likely the target address).  It can store
   * the parsed value anywhere in any way it likes, however it must first call
   * cf_journal_block() on the overwritten memory block.  It returns an error
   * message or NULL if everything is all right.  */
typedef byte *cf_parser1(byte *string, void *ptr);
  /* A parser function for user-defined types gets a string and a pointer to
   * the destination variable.  It must store the value within [ptr,ptr+size),
   * where size is fixed for each type.  It should not call cf_journal_block().  */
typedef byte *cf_hook(void *ptr);
  /* An init- or commit-hook gets a pointer to the section or NULL if this
   * is the global section.  It returns an error message or NULL if everything
   * is all right.  The init-hook should fill in default values (needed for
   * dynamically allocated nodes of link lists or for filling global variables
   * that are run-time dependent).  The commit-hook should perform sanity
   * checks and postprocess the parsed values.  Commit-hooks must call
   * cf_journal_block() too.  Caveat! init-hooks for static sections must not
   * use cf_malloc() but normal xmalloc().  */
typedef void cf_dumper1(struct fastbuf *fb, void *ptr);
  /* Dumps the contents of a variable of a user-defined type.  */
typedef byte *cf_copier(void *dest, void *src);
  /* Similar to init-hook, but it copies attributes from another list node
   * instead of setting the attributes to default values.  You have to provide
   * it if your node contains parsed values and/or sub-lists.  */

struct cf_user_type {
  uns size;				// of the parsed attribute
  byte *name;				// name of the type (for dumping)
  cf_parser1 *parser;			// how to parse it
  cf_dumper1 *dumper;			// how to dump the type
};

struct cf_section;
struct cf_item {
  byte *name;				// case insensitive
  int number;				// length of an array or #parameters of a parser (negative means at most)
  void *ptr;				// pointer to a global variable or an offset in a section
  union cf_union {
    struct cf_section *sec;		// declaration of a section or a list
    cf_parser *par;			// parser function
    byte **lookup;			// NULL-terminated sequence of allowed strings for lookups
    struct cf_user_type *utype;		// specification of the user-defined type
  } u;
  enum cf_class cls:16;			// attribute class
  enum cf_type type:16;			// type of a static or dynamic attribute
};

struct cf_section {
  uns size;				// 0 for a global block, sizeof(struct) for a section
  cf_hook *init;			// fills in default values (no need to bzero)
  cf_hook *commit;			// verifies parsed data (optional)
  cf_copier *copy;			// copies values from another instance (optional, no need to copy basic attributes)
  struct cf_item *cfg;			// CC_END-terminated array of items
  uns flags;				// for internal use only
};

/* Declaration of cf_section */
#define CF_TYPE(s)	.size = sizeof(s)
#define CF_INIT(f)	.init = (cf_hook*) f
#define CF_COMMIT(f)	.commit = (cf_hook*) f
#define CF_COPY(f)	.copy = (cf_copier*) f
#define CF_ITEMS	.cfg = ( struct cf_item[] )
#define CF_END		{ .cls = CC_END }
/* Configuration items */
#define CF_STATIC(n,p,T,t,c)	{ .cls = CC_STATIC, .type = CT_##T, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,t*) }
#define CF_DYNAMIC(n,p,T,t,c)	{ .cls = CC_DYNAMIC, .type = CT_##T, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,t**) }
#define CF_PARSER(n,p,f,c)	{ .cls = CC_PARSER, .name = n, .number = c, .ptr = p, .u.par = (cf_parser*) f }
#define CF_SECTION(n,p,s)	{ .cls = CC_SECTION, .name = n, .number = 1, .ptr = p, .u.sec = s }
#define CF_LIST(n,p,s)		{ .cls = CC_LIST, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,clist*), .u.sec = s }
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
#define CF_LOOKUP(n,p,t)	{ .cls = CC_STATIC, .type = CT_LOOKUP, .name = n, .number = 1, .ptr = CHECK_PTR_TYPE(p,int*), .u.lookup = t }
#define CF_LOOKUP_ARY(n,p,t,c)	{ .cls = CC_STATIC, .type = CT_LOOKUP, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,int*), .u.lookup = t }
#define CF_LOOKUP_DYN(n,p,t,c)	{ .cls = CC_DYNAMIC, .type = CT_LOOKUP, .name = n, .number = c, .ptr = CHECK_PTR_TYPE(p,int**), .u.lookup = t }
#define CF_USER(n,p,t)		{ .cls = CC_STATIC, .type = CT_USER, .name = n, .number = 1, .ptr = p, .u.utype = t }
#define CF_USER_ARY(n,p,t,c)	{ .cls = CC_STATIC, .type = CT_USER, .name = n, .number = c, .ptr = p, .u.utype = t }
#define CF_USER_DYN(n,p,t,c)	{ .cls = CC_DYNAMIC, .type = CT_USER, .name = n, .number = c, .ptr = p, .u.utype = t }
  // Beware that CF_USER_DYN can only be used on user-defined types of size at least 4

/* If you aren't picky about the number of parameters */
#define CF_ANY_NUM		-0x7fffffff

#define DARY_LEN(a) *(uns*)(a-1)
  // length of a dynamic array
#define DARY_ALLOC(type,len,val...) (type[]) { (type)len, ##val } + 1
  // creates a static instance of a dynamic array
  // FIXME: overcast doesn't work for the double type

/* Memory allocation */
struct mempool;
extern struct mempool *cf_pool;
void *cf_malloc(uns size);
void *cf_malloc_zero(uns size);
byte *cf_strdup(byte *s);
byte *cf_printf(char *fmt, ...) FORMAT_CHECK(printf,1,2);

/* Undo journal for error recovery */
extern uns cf_need_journal;
void cf_journal_block(void *ptr, uns len);
#define CF_JOURNAL_VAR(var) cf_journal_block(&(var), sizeof(var))

struct cf_journal_item;
struct cf_journal_item *cf_journal_new_transaction(uns new_pool);
void cf_journal_commit_transaction(uns new_pool, struct cf_journal_item *oldj);
void cf_journal_rollback_transaction(uns new_pool, struct cf_journal_item *oldj);

/* Declaration */
void cf_declare_section(byte *name, struct cf_section *sec, uns allow_unknown);
void cf_init_section(byte *name, struct cf_section *sec, void *ptr, uns do_bzero);

/* Parsers for basic types */
byte *cf_parse_int(byte *str, int *ptr);
byte *cf_parse_u64(byte *str, u64 *ptr);
byte *cf_parse_double(byte *str, double *ptr);
byte *cf_parse_ip(byte *p, u32 *varp);

#endif

