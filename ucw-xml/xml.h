/*
 *	UCW Library -- A simple XML parser
 *
 *	(c) 2007--2008 Pavel Charvat <pchar@ucw.cz>
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_XML_XML_H
#define _UCW_XML_XML_H

#include <ucw/clists.h>
#include <ucw/slists.h>
#include <ucw/mempool.h>
#include <ucw/fastbuf.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define xml_attr_find ucw_xml_attr_find
#define xml_attr_value ucw_xml_attr_value
#define xml_cleanup ucw_xml_cleanup
#define xml_def_find_entity ucw_xml_def_find_entity
#define xml_def_resolve_entity ucw_xml_def_resolve_entity
#define xml_error ucw_xml_error
#define xml_fatal ucw_xml_fatal
#define xml_init ucw_xml_init
#define xml_merge_chars ucw_xml_merge_chars
#define xml_merge_dom_chars ucw_xml_merge_dom_chars
#define xml_next ucw_xml_next
#define xml_next_state ucw_xml_next_state
#define xml_normalize_white ucw_xml_normalize_white
#define xml_parse ucw_xml_parse
#define xml_push_fastbuf ucw_xml_push_fastbuf
#define xml_reset ucw_xml_reset
#define xml_row ucw_xml_row
#define xml_skip_element ucw_xml_skip_element
#define xml_warn ucw_xml_warn
#endif

struct xml_context;
struct xml_dtd_entity;

enum xml_error {
  XML_ERR_OK = 0,
  XML_ERR_WARN = 1000,					/* Warning */
  XML_ERR_ERROR = 2000,					/* Recoverable error */
  XML_ERR_FATAL = 3000,					/* Unrecoverable error */
  XML_ERR_EOF,
};

enum xml_state {
  XML_STATE_EOF,					/* EOF or a fatal error */
  XML_STATE_START,					/* Initial state */
  XML_STATE_XML_DECL,					/* XML_PULL_XML_DECL */
  XML_STATE_DOCTYPE_DECL,				/* XML_PULL_DOCTYPE_DECL */
  XML_STATE_CHARS,					/* XML_PULL_CHARS */
  XML_STATE_STAG,					/* XML_PULL_STAG */
  XML_STATE_ETAG,					/* XML_PULL_ETAG */
  XML_STATE_COMMENT,					/* XML_PULL_COMMENT */
  XML_STATE_PI,						/* XML_PULL_PI */

  /* Internal states */
  XML_STATE_CHARS_BEFORE_STAG,
  XML_STATE_CHARS_BEFORE_ETAG,
  XML_STATE_CHARS_BEFORE_CDATA,
  XML_STATE_CHARS_BEFORE_COMMENT,
  XML_STATE_CHARS_BEFORE_PI,
  XML_STATE_PROLOG_COMMENT,
  XML_STATE_PROLOG_PI,
  XML_STATE_EPILOG_COMMENT,
  XML_STATE_EPILOG_PI,
};

enum xml_pull {
  XML_PULL_XML_DECL =			0x00000001,	/* Stop after the XML declaration */
  XML_PULL_DOCTYPE_DECL =		0x00000002,	/* Stop in the doctype declaration (before optional internal subset) */
  XML_PULL_CHARS =			0x00000004,
  XML_PULL_STAG =			0x00000008,
  XML_PULL_ETAG =			0x00000010,
  XML_PULL_COMMENT =			0x00000020,
  XML_PULL_PI =				0x00000040,
  XML_PULL_ALL =			0xffffffff,
};

enum xml_flags {
  /* Enable reporting of various events via SAX and/or PULL interface */
  XML_REPORT_COMMENTS =			0x00000001,	/* Report comments */
  XML_REPORT_PIS =			0x00000002,	/* Report processing instructions */
  XML_REPORT_CHARS =			0x00000004,	/* Report characters */
  XML_REPORT_TAGS =			0x00000008,	/* Report element starts/ends */
  XML_REPORT_MISC = XML_REPORT_COMMENTS | XML_REPORT_PIS,
  XML_REPORT_ALL = XML_REPORT_MISC | XML_REPORT_CHARS | XML_REPORT_TAGS,

  /* Enable construction of DOM for these types */
  XML_ALLOC_COMMENTS =			0x00000010,	/* Create comment nodes */
  XML_ALLOC_PIS =			0x00000020,	/* Create processing instruction nodes */
  XML_ALLOC_CHARS =			0x00000040,	/* Create character nodes */
  XML_ALLOC_TAGS =			0x00000080,	/* Create element nodes */
  XML_ALLOC_MISC = XML_ALLOC_COMMENTS | XML_ALLOC_PIS,
  XML_ALLOC_ALL = XML_ALLOC_MISC | XML_ALLOC_CHARS | XML_ALLOC_TAGS,

  /* Other parameters */
  XML_VALIDATING =			0x00000100,	/* Validate everything (not fully implemented!) */
  XML_PARSE_DTD =			0x00000200,	/* Enable parsing of DTD */
  XML_NO_CHARS =			0x00000400,	/* The current element must not contain character data (filled automaticaly if using DTD) */
  XML_ALLOC_DEFAULT_ATTRS =		0x00000800,	/* Allocate default attribute values so they can be found by XML_ATTR_FOR_EACH */
  XML_NAMESPACES =			0x00001000,	/* Parse namespaces, use xml_ns_enable() to set this */

  /* Internals, do not change! */
  XML_EMPTY_ELEM_TAG =			0x00010000,	/* The current element match EmptyElemTag */
  XML_VERSION_1_1 =			0x00020000,	/* XML version is 1.1, otherwise 1.0 */
  XML_HAS_EXTERNAL_SUBSET =		0x00040000,	/* The document contains a reference to external DTD subset */
  XML_HAS_INTERNAL_SUBSET =		0x00080000,	/* The document contains an internal subset */
  XML_HAS_DTD =	XML_HAS_EXTERNAL_SUBSET | XML_HAS_INTERNAL_SUBSET,
  XML_SRC_EOF =				0x00100000,	/* EOF reached */
  XML_SRC_EXPECTED_DECL =		0x00200000,	/* Just before optional or required XMLDecl/TextDecl */
  XML_SRC_DOCUMENT =			0x00400000,	/* The document entity */
  XML_SRC_EXTERNAL =			0x00800000,	/* An external entity */
};

enum xml_node_type {
  XML_NODE_ELEM,
  XML_NODE_COMMENT,
  XML_NODE_CHARS,
  XML_NODE_PI,
};

#define XML_NODE_FOR_EACH(var, node) CLIST_FOR_EACH(struct xml_node *, var, (node)->sons)
#define XML_ATTR_FOR_EACH(var, node) SLIST_FOR_EACH(struct xml_attr *, var, (node)->attrs)

struct xml_node {
  cnode n;						/* Node for list of parent's sons */
  uint type;						/* XML_NODE_x */
  struct xml_node *parent;				/* Parent node */
  /*
   *  If namespaces are enabled, node->name points to the local part of the name
   *  and node->ns is the resolved namespace ID.
   *
   *  However, the namespace prefix is kept in memory just before the local part,
   *  so you can use xml_node_qname() to find out the full qualified name.
   *  The same applies to attributes, but the function is xml_attr_qname().
   */
  char *name;						/* Element name / PI target */
  clist sons;						/* Children nodes */
  union {
    struct {
      char *text;					/* PI text / Comment / CDATA */
      uint len;						/* Text length in bytes */
    };
    struct {
      uint ns;						/* Namespace ID */
      struct xml_dtd_elem *dtd;				/* Element DTD */
      slist attrs;					/* Link list of element attributes */
    };
  };
  void *user;						/* User-defined (initialized to NULL) */
};

struct xml_attr {
  snode n;						/* Node for elem->attrs */
  struct xml_node *elem;				/* Parent element */
  struct xml_dtd_attr *dtd;				/* Attribute DTD */
  uint ns;						/* Namespace ID */
  char *name;						/* Attribute name without NS prefix */
  char *val;						/* Attribute value */
  void *user;						/* User-defined (initialized to NULL) */
};

#define XML_BUF_SIZE 32                                 /* At least 8 -- hardcoded */

struct xml_source {
  struct xml_source *next;				/* Link list of pending fastbufs (xml_context.sources) */
  struct fastbuf *fb;					/* Source fastbuf */
  struct fastbuf *wrapped_fb;				/* Original wrapped fastbuf (needed for cleanup) */
  struct fastbuf wrap_fb;				/* Fbmem wrapper */
  u32 buf[2 * XML_BUF_SIZE];				/* Read buffer with Unicode values and categories */
  u32 *bptr, *bstop;					/* Current state of the buffer */
  uint row;						/* File position */
  char *expected_encoding;				/* Initial encoding before any transformation has been made (expected in XMLDecl/TextDecl) */
  char *fb_encoding;					/* Encoding of the source fastbuf */
  char *decl_encoding;					/* Encoding read from the XMLDecl/TextDecl */
  uint refill_cat1;					/* Character categories, which should be directly passed to the buffer */
  uint refill_cat2;					/* Character categories, which should be processed as newlines (possibly in some built-in
							   sequences) */
  void (*refill)(struct xml_context *ctx);		/* Callback to decode source characters to the buffer */
  unsigned short *refill_in_to_x;			/* Libucw-charset input table */
  uint saved_depth;					/* Saved ctx->depth */
  uint pending_0xd;					/* The last read character is 0xD */
};

struct xml_context {
  /* Error handling */
  char *err_msg;					/* Last error message */
  enum xml_error err_code;				/* Last error code */
  void *throw_buf;					/* Where to jump on error */
  void (*h_warn)(struct xml_context *ctx);		/* Warning callback */
  void (*h_error)(struct xml_context *ctx);		/* Recoverable error callback */
  void (*h_fatal)(struct xml_context *ctx);		/* Unrecoverable error callback */

  /* Memory management */
  struct mempool *pool;					/* DOM pool */
  struct mempool *stack;				/* Stack pool (freed as soon as possible) */
  struct xml_stack *stack_list;				/* See xml_push(), xml_pop() */
  uint flags;						/* XML_FLAG_x (restored on xml_pop()) */
  uint depth;						/* Nesting level (for checking of valid source nesting -> valid pushes/pops on memory pools) */
  struct fastbuf chars;					/* Character data / attribute value */
  struct mempool_state chars_state;			/* Mempool state before the current character block has started */
  char *chars_trivial;					/* If not empty, it will be appended to chars */
  void *tab_attrs;					/* Hash table of element attributes */

  /* Input */
  struct xml_source *src;				/* Current source */
  u32 *bptr, *bstop;					/* Buffer with preprocessed characters (validated UCS-4 + category flags) */
  uint cat_chars;					/* Unicode range of supported characters (cdata, attribute values, ...) */
  uint cat_unrestricted;				/* Unrestricted characters (may appear in document/external entities) */
  uint cat_new_line;					/* New line characters */
  uint cat_name;					/* Characters that may appear in names */
  uint cat_sname;					/* Characters that may begin a name */

  /* SAX-like interface */
  void (*h_document_start)(struct xml_context *ctx);	/* Called before entering prolog */
  void (*h_document_end)(struct xml_context *ctx);	/* Called after leaving epilog */
  void (*h_xml_decl)(struct xml_context *ctx);		/* Called after the XML declaration */
  void (*h_doctype_decl)(struct xml_context *ctx);	/* Called in the doctype declaration (before optional internal subset) */
  void (*h_comment)(struct xml_context *ctx);		/* Called after a comment (only with XML_REPORT_COMMENTS) */
  void (*h_pi)(struct xml_context *ctx);		/* Called after a processing instruction (only with XML_REPORT_PIS) */
  void (*h_stag)(struct xml_context *ctx);		/* Called after STag or EmptyElemTag (only with XML_REPORT_TAGS) */
  void (*h_etag)(struct xml_context *ctx);		/* Called before ETag or after EmptyElemTag (only with XML_REPORT_TAGS) */
  void (*h_chars)(struct xml_context *ctx);		/* Called after some characters (only with XML_REPORT_CHARS) */
  void (*h_block)(struct xml_context *ctx, char *text, uint len);	/* Called for each continuous block of characters not reported by h_cdata() (only with XML_REPORT_CHARS) */
  void (*h_cdata)(struct xml_context *ctx, char *text, uint len);	/* Called for each CDATA section (only with XML_REPORT_CHARS) */
  void (*h_ignorable)(struct xml_context *ctx, char *text, uint len);	/* Called for ignorable whitespace (content in tags without #PCDATA) */
  void (*h_dtd_start)(struct xml_context *ctx);		/* Called just after the DTD structure is initialized */
  void (*h_dtd_end)(struct xml_context *ctx);		/* Called after DTD subsets subsets */
  struct xml_dtd_entity *(*h_find_entity)(struct xml_context *ctx, char *name);		/* Called when needed to resolve a general entity */
  void (*h_resolve_entity)(struct xml_context *ctx, struct xml_dtd_entity *ent);	/* User should push source fastbuf for a parsed external entity (either general or parameter) */

  /* DOM */
  struct xml_node *dom;					/* DOM root */
  struct xml_node *node;				/* Current DOM node */

  /* Namespaces */
  struct mempool *ns_pool;				/* Memory pool for NS definitions */
  const char **ns_by_id;				/* A growing array translating NS IDs to their names */
  void *ns_by_name;					/* Hash table translating NS names to their IDs */
  void *ns_by_prefix;					/* Hash table translating current prefixes to NS IDs, allocated from xml->stack */
  struct xml_ns_prefix *ns_prefix_stack;		/* A stack of prefix definitions, allocated from xml->stack */
  uint ns_default;					/* Current default namespace */

  char *version_str;
  uint standalone;
  char *doctype;					/* The document type (or NULL if unknown) */
  char *system_id;					/* DTD external id */
  char *public_id;					/* DTD public id */
  struct xml_dtd *dtd;					/* The DTD structure (or NULL) */
  uint state;						/* Current state for the PULL interface (XML_STATE_x) */
  uint pull;						/* Parameters for the PULL interface (XML_PULL_x) */
};

/* Initialize XML context */
void xml_init(struct xml_context *ctx);

/* Clean up all internal structures */
void xml_cleanup(struct xml_context *ctx);

/* Reuse XML context, equivalent to xml_cleanup() and xml_init() */
void xml_reset(struct xml_context *ctx);

/* Add XML source (fastbuf will be automatically closed) */
struct xml_source *xml_push_fastbuf(struct xml_context *ctx, struct fastbuf *fb);

/* Parse without the PULL interface, return XML_ERR_x code (zero on success) */
uint xml_parse(struct xml_context *ctx);

/* Parse with the PULL interface, return XML_STATE_x (zero on EOF or fatal error) */
uint xml_next(struct xml_context *ctx);

/* Equivalent to xml_next, but with temporarily changed ctx->pull value */
uint xml_next_state(struct xml_context *ctx, uint pull);

/* May be called on XML_STATE_STAG to skip it's content; can return XML_STATE_ETAG or XML_STATE_EOF on fatal error */
uint xml_skip_element(struct xml_context *ctx);

/* Returns the current row number in the document entity */
uint xml_row(struct xml_context *ctx);

/* Finds a qualified name (including namespace prefix) of a given element node. */
char *xml_node_qname(struct xml_context *ctx, struct xml_node *node);

/* Finds a qualified name (including namespace prefix) of a given attribute. */
char *xml_attr_qname(struct xml_context *ctx, struct xml_attr *node);

/* Finds a given attribute value in a XML_NODE_ELEM node */
struct xml_attr *xml_attr_find(struct xml_context *ctx, struct xml_node *node, char *name);

/* The same, but namespace-aware */
struct xml_attr *xml_attr_find_ns(struct xml_context *ctx, struct xml_node *node, uint ns, char *name);

/* Similar to xml_attr_find, but it deals also with default values */
char *xml_attr_value(struct xml_context *ctx, struct xml_node *node, char *name);

/* The default value of h_find_entity(), knows &lt;, &gt;, &amp;, &apos; and &quot; */
struct xml_dtd_entity *xml_def_find_entity(struct xml_context *ctx, char *name);

/* The default value of h_resolve_entity(), throws an error */
void xml_def_resolve_entity(struct xml_context *ctx, struct xml_dtd_entity *ent);

/* Remove leading/trailing spaces and replaces sequences of spaces to a single space character (non-CDATA attribute normalization) */
uint xml_normalize_white(struct xml_context *ctx, char *value);

/* Merge character contents of a given element to a single string (not recursive) */
char *xml_merge_chars(struct xml_context *ctx, struct xml_node *node, struct mempool *pool);

/* Merge character contents of a given subtree to a single string */
char *xml_merge_dom_chars(struct xml_context *ctx, struct xml_node *node, struct mempool *pool);

/* Public part of error handling */
void xml_warn(struct xml_context *ctx, const char *format, ...);
void xml_error(struct xml_context *ctx, const char *format, ...);
void NONRET xml_fatal(struct xml_context *ctx, const char *format, ...);

/* Request processing of namespaces */
void xml_ns_enable(struct xml_context *ctx);

/* Looks up namespace by its ID, dies on an invalid ID */
const char *xml_ns_by_id(struct xml_context *ctx, uint ns);

/* Looks up namespace by its name and returns its ID. Creates a new ID if necessary. */
uint xml_ns_by_name(struct xml_context *ctx, const char *name);

/* Well-known namespaces */
#define XML_NS_NONE		0	/* This element has no namespace */
#define XML_NS_XMLNS		1	/* xmlns: */
#define XML_NS_XML		2	/* xml: */

#endif
