/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007--2008 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_XML_DTD_H
#define _SHERLOCK_XML_DTD_H

#include <xml/xml.h>

struct xml_dtd {
  struct mempool *pool;			/* Memory pool where to allocate DTD */
  slist ents;				/* Link list of general entities */
  slist pents;				/* Link list of parameter entities */
  slist notns;				/* Link list of notations */
  slist elems;				/* Link list of elements */
  void *tab_ents;			/* Hash table of general entities */
  void *tab_pents;			/* Hash table of parameter entities */
  void *tab_notns;			/* Hash table of notations */
  void *tab_elems;			/* Hash table of elements */
  void *tab_enodes;			/* Hash table of element sons */
  void *tab_attrs;			/* Hash table of element attributes */
  void *tab_evals;			/* Hash table of enumerated attribute values */
  void *tab_enotns;			/* hash table of enumerated attribute notations */
};

/* Notations */

enum xml_dtd_notn_flags {
  XML_DTD_NOTN_DECLARED = 0x1,		/* The notation has been declared (internal usage) */
};

struct xml_dtd_notn {
  snode n;				/* Node in xml_dtd.notns */
  uint flags;				/* XML_DTD_NOTN_x */
  char *name;				/* Notation name */
  char *system_id;			/* External ID */
  char *public_id;
  void *user;				/* User-defined */
};

struct xml_dtd_notn *xml_dtd_find_notn(struct xml_context *ctx, char *name);

/* Entities */

enum xml_dtd_entity_flags {
  XML_DTD_ENTITY_DECLARED = 0x1,	/* The entity has been declared (internal usage) */
  XML_DTD_ENTITY_VISITED = 0x2,		/* Cycle detection (internal usage) */
  XML_DTD_ENTITY_PARAMETER = 0x4,	/* Parameter entity, general otherwise */
  XML_DTD_ENTITY_EXTERNAL = 0x8,	/* External entity, internal otherwise */
  XML_DTD_ENTITY_UNPARSED = 0x10,	/* Unparsed entity, parsed otherwise */
  XML_DTD_ENTITY_TRIVIAL = 0x20,	/* Replacement text is a sequence of characters and character references */
};

struct xml_dtd_entity {
  snode n;				/* Node in xml_dtd.[gp]ents */
  uint flags;				/* XML_DTD_ENT_x */
  char *name;				/* Entity name */
  char *text;				/* Replacement text / expanded replacement text (XML_DTD_ENT_TRIVIAL) */
  uint len;				/* Text length */
  char *system_id;			/* External ID */
  char *public_id;
  struct xml_dtd_notn *notn;		/* Notation (XML_DTD_ENT_UNPARSED only) */
  void *user;				/* User-defined */
};

struct xml_dtd_entity *xml_dtd_find_entity(struct xml_context *ctx, char *name);

/* Elements */

enum xml_dtd_elem_flags {
  XML_DTD_ELEM_DECLARED = 0x1,		/* The element has been declared (internal usage) */
};

enum xml_dtd_elem_type {
  XML_DTD_ELEM_EMPTY,
  XML_DTD_ELEM_ANY,
  XML_DTD_ELEM_MIXED,
  XML_DTD_ELEM_CHILDREN,
};

struct xml_dtd_elem {
  snode n;
  uint flags;
  uint type;
  char *name;
  struct xml_dtd_elem_node *node;
  slist attrs;
  void *user;				/* User-defined */
};

struct xml_dtd_elem_node {
  snode n;
  struct xml_dtd_elem_node *parent;
  struct xml_dtd_elem *elem;
  slist sons;
  uint type;
  uint occur;
  void *user;				/* User-defined */
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

struct xml_dtd_elem *xml_dtd_find_elem(struct xml_context *ctx, char *name);

/* Attributes */

enum xml_dtd_attr_default {
  XML_ATTR_NONE,
  XML_ATTR_REQUIRED,
  XML_ATTR_IMPLIED,
  XML_ATTR_FIXED,
};

enum xml_dtd_attr_type {
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
  snode n;
  char *name;				/* Attribute name */
  struct xml_dtd_elem *elem;		/* Owner element */
  uint type;				/* See enum xml_dtd_attr_type */
  uint default_mode;			/* See enum xml_dtd_attr_default */
  char *default_value;			/* The default value defined in DTD (or NULL) */
};

struct xml_dtd_eval {
  struct xml_dtd_attr *attr;
  char *val;
};

struct xml_dtd_enotn {
  struct xml_dtd_attr *attr;
  struct xml_dtd_notn *notn;
};

void xml_dtd_init(struct xml_context *ctx);
void xml_dtd_cleanup(struct xml_context *ctx);
void xml_dtd_finish(struct xml_context *ctx);

struct xml_dtd_attr *xml_dtd_find_attr(struct xml_context *ctx, struct xml_dtd_elem *elem, char *name);

#endif
