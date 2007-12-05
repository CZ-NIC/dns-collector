/*
 *	UCW Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_XML_H
#define _UCW_XML_H

#include "lib/clists.h"
#include "lib/slists.h"

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
  XML_FLAG_VERSION_1_1 = 0x2,
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

#define XML_BUF_SIZE 32

struct xml_source {
  struct xml_source *next;			/* Link list of pending fastbufs (xml_context.sources) */
  struct fastbuf *fb;
  u32 buf[2 * XML_BUF_SIZE];			/* Read buffer with Unicode values and categories */
  u32 *bptr, *bstop;				/* Current state of the buffer */
  uns depth;
  uns flags;
};

enum xml_source_flags {
  XML_SRC_DECL = 0x1,				/* Expected document/text declaration */
  XML_SRC_EOF = 0x2,				/* Reached the end of the fastbuf */
  XML_SRC_NEW_LINE = 0x4,			/* The last read character is 0xD */
  XML_SRC_SURROUND = 0x8,			/* Surround the text with 0x20 (references to parameter entities) */
  XML_SRC_DOCUMENT = 0x10,			/* The document entity */
  XML_SRC_EXTERNAL = 0x20,			/* An external entity */
};

#if 0
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
#endif

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

  /* Input */
  struct xml_source *sources;				/* Stack of pending sources */
  u32 *bptr, *bstop;					/* Character buffer */
  uns depth;						/* Nesting level */

  /* SAX-like interface */
  void (*h_document_start)(struct xml_context *ctx);	/* Called before entering prolog */
  void (*h_document_end)(struct xml_context *ctx);	/* Called after leaving epilog */
  void (*h_xml_decl)(struct xml_context *ctx);		/* Called after the XML declaration */
  void (*h_doctype_decl)(struct xml_context *ctx);	/* Called in the doctype declaration just before internal subset */
  void (*h_pi)(struct xml_context *ctx);		/* Called after a processing instruction */
  void (*h_comment)(struct xml_context *ctx);		/* Called after a comment */

  /* */
  struct xml_node *node;				/* Current XML node */
  uns flags;						/* XML_FLAG_x */
  struct xml_element *element;				/* Current element */
  void *attribute_table;
  char *version_str;
  char *encoding;
  uns standalone;
  char *document_type;
  struct xml_dtd *dtd;
  struct xml_ext_id eid;
  uns state;
  uns want;

  void (*start_dtd)(struct xml_context *ctx);
  void (*end_dtd)(struct xml_context *ctx);
  void (*start_element)(struct xml_context *ctx);
  void (*end_element)(struct xml_context *ctx);
  void (*start_cdata)(struct xml_context *ctx);
  void (*end_cdata)(struct xml_context *ctx);
  void (*start_entity)(struct xml_context *ctx);
  void (*end_entity)(struct xml_context *ctx);
  void (*chacacters)(struct xml_context *ctx);
  struct fastbuf *(*resolve_entity)(struct xml_context *ctx);
  void (*notation_decl)(struct xml_context *ctx);
  void (*unparsed_entity_decl)(struct xml_context *ctx);
};

struct xml_attribute {
  char *name;
  char *value;
  struct xml_element *element;
  struct xml_attribute *next;
  struct xml_dtd_attribute *dtd;
};

struct xml_element {
  char *name;
  struct xml_attribute *attrs;
  struct xml_element *parent;
  struct xml_dtd_element *dtd;
};

/*** Document Type Definition (DTD) ***/

struct xml_dtd {
  slist gents;				/* Link list of general entities */
  slist pents;				/* Link list of parapeter entities */
  slist notns;				/* Link list of notations */
  slist elems;				/* Link list of elements */
  void *tab_gents;			/* Hash table of general entities */
  void *tab_pents;			/* Hash table of parameter entities */
  void *tab_notns;			/* Hash table of notations */
  void *tab_elems;			/* Hash table of elements */
  void *tab_attrs;			/* Hash table of element attributes */
  void *tab_evals;			/* Hash table of enumerated attribute values */
  void *tab_enotns;			/* hash table of enumerated attribute notations */
};

/* Notations */

enum xml_dtd_notn_flags {
  XML_DTD_NOTN_DECLARED = 0x1,		/* The notation has been declared (interbal usage) */
};

struct xml_dtd_notn {
  snode n;				/* Node in xml_dtd.notns */
  uns flags;				/* XML_DTD_NOTN_x */
  char *name;				/* Notation name */
  struct xml_ext_id eid;		/* External id */
};

/* Entities */

enum xml_dtd_ent_flags {
  XML_DTD_ENT_DECLARED = 0x1,		/* The entity has been declared (internal usage) */
  XML_DTD_ENT_VISITED = 0x2,		/* Cycle detection (internal usage) */
  XML_DTD_ENT_PARAMETER = 0x4,		/* Parameter entity, general otherwise */
  XML_DTD_ENT_EXTERNAL = 0x8,		/* External entity, internal otherwise */
  XML_DTD_ENT_UNPARSED = 0x10,		/* Unparsed entity, parsed otherwise */
  XML_DTD_ENT_TRIVIAL = 0x20,		/* Replacement text is a sequence of characters and character references */
};

struct xml_dtd_ent {
  snode n;				/* Node in xml_dtd.[gp]ents */
  uns flags;				/* XML_DTD_ENT_x */
  char *name;				/* Entity name */
  char *text;				/* Replacement text / expanded replacement text (XML_DTD_ENT_TRVIAL) */
  uns len;				/* Text length */
  struct xml_ext_id eid;		/* External ID */
  struct xml_dtd_notn *notn;		/* Notation (XML_DTD_ENT_UNPARSED only) */
};

/* Elements */

enum xml_dtd_elem_flags {
  XML_DTD_ELEM_DECLARED = 0x1,		/* The element has been declared (internal usage) */
};

struct xml_dtd_elem {
  snode n;
  uns flags;
  char *name;
  struct xml_dtd_elem_node *node;
};

struct xml_dtd_elem_node {
  snode n;
  struct xml_dtd_elem_node *parent;
  slist sons;
  uns type;
  uns occur;
};

enum xml_dtd_elem_node_type {
  XML_DTD_ELEM_PCDATA,
  XML_DTD_ELEM_SEQ,
  XML_DTD_ELEM_OR,
};

enum xml_dtd_elem_node_occur {
  XML_DTD_ELEM_OCCUR_ONCE,
  XML_DTD_ELEM_OCCUR_OPT,
  XML_DTD_ELEM_OCCUR_MULT,
  XML_DTD_ELEM_OCCUR_PLUS,
};

/* Attributes */


enum xml_dtd_attribute_default {
  XML_ATTR_NONE,
  XML_ATTR_REQUIRED,
  XML_ATTR_IMPLIED,
  XML_ATTR_FIXED,
};

enum xml_dtd_attribute_type {
  XML_ATTR_CDATA,
  XML_ATTR_ID,
  XML_ATTR_IDREF,
  XML_ATTR_IDREFS,
  XML_ATTR_ENTITY,
  XML_ATTR_ENTITIES,
  XML_ATTR_NMTOKEN,
  XML_ATTR_NMTOKENS,
  XML_ATTR_ENUM,
  XML_ATTR_NOTATION,
};

struct xml_dtd_attr {
  char *name;
  struct xml_dtd_elem *elem;
  enum xml_dtd_attribute_type type;
  enum xml_dtd_attribute_default default_mode;
  char *default_value;
};

struct xml_dtd_eval {
  struct xml_dtd_attr *attr;
  char *val;
};

struct xml_dtd_enotn {
  struct xml_dtd_attr *attr;
  struct xml_dtd_notn *notn;
};

void xml_init(struct xml_context *ctx);
void xml_cleanup(struct xml_context *ctx);
void xml_set_source(struct xml_context *ctx, struct fastbuf *fb);
int xml_next(struct xml_context *ctx);

#endif
