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

enum xml_error {
  XML_ERR_OK = 0,
  XML_ERR_WARN = 1000,				/* Warning */
  XML_ERR_ERROR = 2000,				/* Recoverable error */
  XML_ERR_FATAL = 3000,				/* Unrecoverable error */
  XML_ERR_EOF,
};

enum xml_state {
  XML_STATE_START = 0,
  XML_STATE_DECL,
  XML_STATE_DOCUMENT_TYPE,
  XML_STATE_CHARS,
  XML_STATE_WHITE,
  XML_STATE_CDATA,
  XML_STATE_STAG,
  XML_STATE_ETAG,
  XML_STATE_COMMENT,
  XML_STATE_PI,
  XML_STATE_EOF,
  XML_STATE_FATAL,

  /* Internal states */
  XML_STATE_CHARS_BEFORE_STAG,
  XML_STATE_CHARS_BEFORE_ETAG,
  XML_STATE_CHARS_BEFORE_CDATA,
  XML_STATE_CHARS_BEFORE_PI,
  XML_STATE_CHARS_BEFORE_COMMENT,
  XML_STATE_PROLOG_PI,
  XML_STATE_PROLOG_COMMENT,
  XML_STATE_EPILOG_PI,
  XML_STATE_EPILOG_COMMENT,
};

enum xml_want {
  XML_WANT_DECL = 1 << XML_STATE_DECL,
  XML_WANT_DOCUMENT_TYPE = 1 << XML_STATE_DOCUMENT_TYPE,
  XML_WANT_CHARS = 1 << XML_STATE_CHARS,
  XML_WANT_WHITE = 1 << XML_STATE_WHITE,
  XML_WANT_CDATA = 1 << XML_STATE_CDATA,
  XML_WANT_STAG = 1 << XML_STATE_STAG,
  XML_WANT_ETAG = 1 << XML_STATE_ETAG,
  XML_WANT_COMMENT = 1 << XML_STATE_COMMENT,
  XML_WANT_PI = 1 << XML_STATE_PI,
  XML_WANT_EOF = 1 << XML_STATE_EOF,
  XML_WANT_ALL = ~0U,
};

enum xml_flags {
  XML_FLAG_VALIDATING = 0x1,
  XML_FLAG_VERSION_1_1 = 0x2,			/* XML version 1.1, otherwise 1.0 */
  XML_FLAG_HAS_EXTERNAL_SUBSET = 0x4,		/* The document contains a reference to external DTD subset */
  XML_FLAG_HAS_INTERNAL_SUBSET = 0x8,		/* The document contains an internal subset */

  XML_FLAG_SRC_EOF = 0x10,			/* EOF reached */
  XML_FLAG_SRC_EXPECTED_DECL = 0x20,		/* Just before optional or required XMLDecl/TextDecl */
  XML_FLAG_SRC_NEW_LINE = 0x40,			/* The last read character is 0xD */
  XML_FLAG_SRC_SURROUND = 0x80,			/* Surround the text with 0x20 (references to parameter entities) */
  XML_FLAG_SRC_DOCUMENT = 0x100,		/* The document entity */
  XML_FLAG_SRC_EXTERNAL = 0x200,		/* An external entity */

  XML_DOM_SKIP = 0x1000,			/* Do not report DOM nodes */
  XML_DOM_FREE = 0x2000,			/* Free the subtree when leaving */
  XML_DOM_IGNORE = XML_DOM_SKIP | XML_DOM_FREE,	/* Completely ignore the subtree */

  XML_FLAG_EMPTY_ELEM = 0x100000,
};

struct xml_ext_id {
  char *system_id;
  char *public_id;
};

enum xml_node_type {
  XML_NODE_ELEM,
  XML_NODE_COMMENT,
  XML_NODE_CDATA,
  XML_NODE_PI,
};

struct xml_node {
  cnode n;					/* Node for list of parent's sons */
  uns type;					/* XML_NODE_x */
  struct xml_node *parent;			/* Parent node */
};

struct xml_elem {
  struct xml_node node;
  char *name;					/* Element name */
  clist sons;					/* List of subnodes */
  struct xml_dtd_elem *dtd;			/* Element DTD */
  slist attrs;					/* Link list of attributes */
};

struct xml_attr {
  snode n;
  struct xml_elem *elem;
  char *name;
  char *val;
};

struct xml_context;

struct xml_stack {
  struct xml_stack *next;			/* Link list of stack records */
  uns saved_flags;				/* Saved ctx->flags */
  struct mempool_state saved_pool;		/* Saved ctx->pool state */
};

#define XML_BUF_SIZE 32				/* At least 16 -- hardcoded */

struct xml_source {
  struct xml_source *next;			/* Link list of pending fastbufs (xml_context.sources) */
  struct fastbuf *fb;				/* Source fastbuf */
  struct fastbuf wrap_fb;			/* Libcharset or fbmem wrapper */
  u32 buf[2 * XML_BUF_SIZE];			/* Read buffer with Unicode values and categories */
  u32 *bptr, *bstop;				/* Current state of the buffer */
  uns row;					/* File position */
  char *expected_encoding;			/* Initial encoding before any transformation has been made (expected in XMLDecl/TextDecl) */
  char *fb_encoding;				/* Encoding of the source fastbuf */
  char *decl_encoding;				/* Encoding read from the XMLDecl/TextDecl */
  uns refill_cat1;				/* Character categories, which should be directly passed to the buffer */
  uns refill_cat2;				/* Character categories, which should be processed as newlines (possibly in some built-in sequences) */
  void (*refill)(struct xml_context *ctx);	/* Callback to decode source characters to the buffer */
  unsigned short *refill_in_to_x;		/* Libcharset input table */
  uns saved_depth;				/* Saved ctx->depth */
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
  struct mempool *pool;					/* Most data */
  struct fastbuf *chars;				/* Character data */
  struct fastbuf *value;				/* Attribute value / comment / processing instruction data */
  char *name;						/* Attribute name, processing instruction target */
  void *tab_attrs;

  /* Stack */
  struct xml_stack *stack;				/* See xml_push(), xml_pop() */
  uns flags;						/* XML_FLAG_x (restored on xml_pop()) */
  uns depth;						/* Nesting level */

  /* Input */
  struct xml_source *src;				/* Current source */
  u32 *bptr, *bstop;					/* Character buffer */

  /* SAX-like interface */
  void (*h_document_start)(struct xml_context *ctx);	/* Called before entering prolog */
  void (*h_document_end)(struct xml_context *ctx);	/* Called after leaving epilog */
  void (*h_xml_decl)(struct xml_context *ctx);		/* Called after the XML declaration */
  void (*h_doctype_decl)(struct xml_context *ctx);	/* Called in the doctype declaration just before internal subset */
  void (*h_pi)(struct xml_context *ctx);		/* Called after a processing instruction */
  void (*h_comment)(struct xml_context *ctx);		/* Called after a comment */
  void (*h_element_start)(struct xml_context *ctx);	/* Called after STag or EmptyElemTag */
  void (*h_element_end)(struct xml_context *ctx);	/* Called before ETag or after EmptyElemTag */

  /* DOM */
  struct xml_elem *root;				/* DOM root */
  union {
    struct xml_node *node;				/* Current DOM node */
    struct xml_elem *elem;				/* Current element */
  };

  char *version_str;
  uns standalone;
  char *document_type;
  struct xml_dtd *dtd;
  struct xml_ext_id eid;
  uns state;
  uns want;

  void (*start_dtd)(struct xml_context *ctx);
  void (*end_dtd)(struct xml_context *ctx);
  void (*start_cdata)(struct xml_context *ctx);
  void (*end_cdata)(struct xml_context *ctx);
  void (*start_entity)(struct xml_context *ctx);
  void (*end_entity)(struct xml_context *ctx);
  void (*chacacters)(struct xml_context *ctx);
  struct fastbuf *(*resolve_entity)(struct xml_context *ctx);
  void (*notation_decl)(struct xml_context *ctx);
  void (*unparsed_entity_decl)(struct xml_context *ctx);
};

void xml_init(struct xml_context *ctx);
void xml_cleanup(struct xml_context *ctx);
void xml_set_source(struct xml_context *ctx, struct fastbuf *fb);
int xml_next(struct xml_context *ctx);

#endif
