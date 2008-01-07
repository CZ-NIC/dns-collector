/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_XML_XML_H
#define _SHERLOCK_XML_XML_H

#include "lib/clists.h"
#include "lib/slists.h"
#include "lib/mempool.h"
#include "lib/fastbuf.h"

struct xml_context;
struct xml_dtd_ent;

enum xml_error {
  // FIXME
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
  XML_STATE_CDATA,					/* XML_PULL_CDATA */
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
  XML_PULL_CDATA =			0x00000008,
  XML_PULL_STAG =			0x00000010,
  XML_PULL_ETAG =			0x00000020,
  XML_PULL_COMMENT =			0x00000040,
  XML_PULL_PI =				0x00000080,
  XML_PULL_ALL =			0xffffffff,
};

enum xml_flags {
  /* Enable reporting of various events via SAX and/or PUSH interface */
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
  XML_UNFOLD_CDATA =			0x00000100,	/* Unfold CDATA sections */
  XML_VALIDATING =			0x00000200,	/* Validate everything (not fully implemented!) */
  XML_PARSE_DTD =			0x00000400,	/* Enable parsing of DTD */

  /* Internals, do not change! */
  XML_EMPTY_ELEM_TAG =			0x00010000,	/* The current element match EmptyElemTag */
  XML_VERSION_1_1 =			0x00020000,	/* XML version is 1.1, otherwise 1.0 */
  XML_HAS_EXTERNAL_SUBSET =		0x00040000,	/* The document contains a reference to external DTD subset */
  XML_HAS_INTERNAL_SUBSET =		0x00080000,	/* The document contains an internal subset */
  XML_SRC_EOF =				0x00100000,	/* EOF reached */
  XML_SRC_EXPECTED_DECL =		0x00200000,	/* Just before optional or required XMLDecl/TextDecl */
  XML_SRC_NEW_LINE =			0x00400000,	/* The last read character is 0xD */
  XML_SRC_SURROUND =			0x00800000,	/* Surround the text with 0x20 (references to parameter entities) */
  XML_SRC_DOCUMENT =			0x01000000,	/* The document entity */
  XML_SRC_EXTERNAL =			0x02000000,	/* An external entity */
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
  uns type;						/* XML_NODE_x */
  struct xml_node *parent;				/* Parent node */
  char *name;						/* Element name / PI target */
  clist sons;						/* Children nodes */
  union {
    struct {
      char *text;					/* PI text / Comment / CDATA */
      uns len;						/* Text length in bytes */
    };
    struct {
      struct xml_dtd_elem *dtd;				/* Element DTD */
      slist attrs;					/* Link list of element attributes */
    };
  };
  void *user;						/* User-defined (initialized to NULL) */
};

struct xml_attr {
  snode n;						/* Node for elem->attrs */
  struct xml_node *elem;				/* Parent element */
  char *name;						/* Attribute name */
  char *val;						/* Attribute value */
  void *user;						/* User-defined (initialized to NULL) */
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
  struct mempool *stack;				/* Stack pool (free as soon as possible) */
  struct xml_stack *stack_list;				/* See xml_push(), xml_pop() */
  uns flags;						/* XML_FLAG_x (restored on xml_pop()) */
  uns depth;						/* Nesting level */
  struct fastbuf chars;					/* Character data / attribute value */
  void *tab_attrs;					/* Hash table of element attributes */

  /* Input */
  struct xml_source *src;				/* Current source */
  u32 *bptr, *bstop;					/* Buffer with preprocessed characters (validated UCS-4 + category flags) */
  uns cat_chars;					/* Unicode range of supported characters (cdata, attribute values, ...) */
  uns cat_unrestricted;					/* Unrestricted characters (may appear in document/external entities) */
  uns cat_new_line;					/* New line characters */
  uns cat_name;						/* Characters that may appear in names */
  uns cat_sname;					/* Characters that may begin a name */

  /* SAX-like interface */
  void (*h_document_start)(struct xml_context *ctx);	/* Called before entering prolog */
  void (*h_document_end)(struct xml_context *ctx);	/* Called after leaving epilog */
  void (*h_xml_decl)(struct xml_context *ctx);		/* Called after the XML declaration */
  void (*h_doctype_decl)(struct xml_context *ctx);	/* Called in the doctype declaration (before optional internal subset) */
  void (*h_comment)(struct xml_context *ctx);		/* Called after a comment (only with XML_REPORT_COMMENTS) */
  void (*h_pi)(struct xml_context *ctx);			/* Called after a processing instruction (only with XML_REPORT_PIS) */
  void (*h_stag)(struct xml_context *ctx);		/* Called after STag or EmptyElemTag (only with XML_REPORT_TAGS) */
  void (*h_etag)(struct xml_context *ctx);		/* Called before ETag or after EmptyElemTag (only with XML_REPORT_TAGS) */
  void (*h_chars)(struct xml_context *ctx);		/* Called after some characters (only with XML_REPORT_CHARS) */
  void (*h_cdata)(struct xml_context *ctx);		/* Called after a CDATA section (only with XML_REPORT_CHARS and XML_UNFOLD_CDATA) */
  void (*h_dtd_start)(struct xml_context *ctx);		/* Called just after the DTD structure is initialized */
  void (*h_dtd_end)(struct xml_context *ctx);		/* Called after DTD subsets subsets */
  struct xml_dtd_ent *(*h_resolve_entity)(struct xml_context *ctx, char *name);

  /* DOM */
  struct xml_node *dom;					/* DOM root */
  struct xml_node *node;				/* Current DOM node */

  char *version_str;
  uns standalone;
  char *doctype;					/* The document type (or NULL if unknown) */
  char *system_id;					/* DTD external id */
  char *public_id;					/* DTD public id */
  struct xml_dtd *dtd;					/* The DTD structure (or NULL) */
  uns state;						/* Current state for the PULL interface (XML_STATE_x) */
  uns pull;						/* Parameters for the PULL interface (XML_PULL_x) */

  void (*start_entity)(struct xml_context *ctx);
  void (*end_entity)(struct xml_context *ctx);
  void (*notation_decl)(struct xml_context *ctx);
  void (*unparsed_entity_decl)(struct xml_context *ctx);
};

/* Initialize XML context */
void xml_init(struct xml_context *ctx);

/* Clean up all internal structures */
void xml_cleanup(struct xml_context *ctx);

/* Reuse XML context */
void xml_reset(struct xml_context *ctx);

/* Setup XML source (fastbuf will be automatically closed) */
void xml_set_source(struct xml_context *ctx, struct fastbuf *fb);

/* Parse without the PUSH interface, return XML_ERR_x code (zero on success) */
uns xml_parse(struct xml_context *ctx);

/* Parse with the PUSH interface, return XML_STATE_x (zero on EOF or fatal error) */
uns xml_next(struct xml_context *ctx);

uns xml_row(struct xml_context *ctx);
struct xml_attr *xml_attr_find(struct xml_context *ctx, struct xml_node *node, char *name);

#endif
