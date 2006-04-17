/*
 *	UCW Library -- Reading of configuration files
 *
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_LIB_CONF2_H
#define	_LIB_CONF2_H

enum cf_type {
  CT_END,				// end of list
  CT_INT, CT_U64, CT_DOUBLE,		// number types
  CT_STRING,				// string type
  CT_FUNCTION,				// arbitrary parser function
  CT_SUB_SECTION,			// sub-section appears exactly once
  CT_LINK_LIST				// link-list with 0..many nodes
};

struct cf_section;
typedef byte *cf_hook(void *sec_ptr, struct cf_section *sec);
  /* An init- or commit-hook gets a pointer to the sub-section or NULL if this
   * is the global section, and a declaration of the section it is called on.
   * It returns an error message or NULL if everything is all right.  */
typedef byte *cf_parser(uns nr_pars, byte **pars, void *sec_ptr, struct cf_section *sec, uns index);
  /* A parser function gets an array of strings and stores it in any way it
   * likes into its own data structures.  It gets a pointer to the sub-section
   * or NULL if this is the global section, a declaration of the section it is
   * called in, and the index of the item it is called on.  It returns an error
   * message or NULL if everything is all right.  */

struct cf_item {
  enum cf_type type;
  byte *name;
  int number;				// number of values: k>0 means exactly k, k<0 means at most k
  void *ptr;				// pointer to a global variable or an offset in a sub-section
  struct cf_section *sub;		// declaration of a sub-section or link-list
};

struct cf_section {
  uns size;				// 0 for a global block, sizeof(struct) for a sub-section
  cf_hook *init;			// fills in default values
  cf_hook *commit;			// verifies parsed data and checks ranges (optional)
  struct cf_item *cfg;			// CT_END-terminated array of items
};

#define CHECK_VAR_TYPE(x,type) ((x)-(type)0 + (type)0)
  // for a pointer x it returns x, and performs a compile-time check whether typeof(x)==type
#define DEFAULT_ARRAY(type,len,val...) (type[]) { (type)len, ##val } + 1
  // creates an array with an allocated space in the front for the (Pascal-like) length
#define CF_FIELD(str,f)	&((str*)0)->f
  // returns a pointer to a field inside a structure suitable for passing as cf_item->ptr

#define CF_END		{ .type = CT_END }
  // please better put this at the end of each section
#define CF_INT(n,p)	{ .type = CT_INT, .name = n, .number = 1, .ptr = CHECK_VAR_TYPE(p,int*) }
#define CF_U64(n,p)	{ .type = CT_U64, .name = n, .number = 1, .ptr = CHECK_VAR_TYPE(p,u64*) }
#define CF_DOUBLE(n,p)	{ .type = CT_DOUBLE, .name = n, .number = 1, .ptr = CHECK_VAR_TYPE(p,double*) }
#define CF_STRING(n,p)	{ .type = CT_STRING, .name = n, .number = 1, .ptr = CHECK_VAR_TYPE(p,byte**) }
#define CF_FUNCTION(n,p)	{ .type = CT_FUNCTION, .name = n, .number = 1, .ptr = CHECK_VAR_TYPE(p,cf_parser*) }
#define CF_SUB_SECTION(n,p,s)	{ .type = CT_SUB_SECTION, .name = n, .number = 1, .ptr = p, .sub = s }
#define CF_LINK_LIST(n,p,s)	{ .type = CT_LINK_LIST, .name = n, .number = 1, .ptr = CHECK_VAR_TYPE(p,struct cnode*), .sub = s }
  // use the macros above to declare configuration items for single variables
#define CF_INT_AR(n,p,c)	{ .type = CT_INT, .name = n, .number = c, .ptr = CHECK_VAR_TYPE(p,int**) }
#define CF_U64_AR(n,p,c)	{ .type = CT_U64, .name = n, .number = c, .ptr = CHECK_VAR_TYPE(p,u64**) }
#define CF_DOUBLE_AR(n,p,c)	{ .type = CT_DOUBLE, .name = n, .number = c, .ptr = CHECK_VAR_TYPE(p,double**) }
#define CF_STRING_AR(n,p,c)	{ .type = CT_STRING, .name = n, .number = c, .ptr = CHECK_VAR_TYPE(p,byte***) }
  // use the macros above to declare configuration items for arrays of variables

#endif
