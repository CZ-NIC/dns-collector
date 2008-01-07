/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "sherlock/xml/xml.h"
#include "sherlock/xml/dtd.h"
#include "sherlock/xml/common.h"
#include "lib/fastbuf.h"
#include "lib/ff-unicode.h"

/* Notations */

#define HASH_PREFIX(x) xml_dtd_notns_##x
#define HASH_NODE struct xml_dtd_notn
#define HASH_KEY_STRING name
#define HASH_ZERO_FILL
#define HASH_TABLE_DYNAMIC
#define HASH_WANT_LOOKUP
#define HASH_GIVE_ALLOC
#define HASH_TABLE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

/* General entities */

#define HASH_PREFIX(x) xml_dtd_ents_##x
#define HASH_NODE struct xml_dtd_ent
#define HASH_KEY_STRING name
#define HASH_ZERO_FILL
#define HASH_TABLE_DYNAMIC
#define HASH_WANT_FIND
#define HASH_WANT_LOOKUP
#define HASH_GIVE_ALLOC
#define HASH_TABLE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

static struct xml_dtd_ent *
xml_dtd_declare_trivial_ent(struct xml_context *ctx, char *name, uns uni)
{
  struct xml_dtd *dtd = ctx->dtd;
  struct xml_dtd_ent *ent = xml_dtd_ents_lookup(dtd->tab_ents, name);
  if (ent->flags & XML_DTD_ENT_DECLARED)
    {
      xml_warn(ctx, "Entity &%s; already declared", name);
      return NULL;
    }
  slist_add_tail(&dtd->ents, &ent->n);
  ent->flags = XML_DTD_ENT_DECLARED | XML_DTD_ENT_TRIVIAL_UNI;
  ent->uni = uni;
  return ent;
}

static void
xml_dtd_declare_default_ents(struct xml_context *ctx)
{
  xml_dtd_declare_trivial_ent(ctx, "lt", 60);
  xml_dtd_declare_trivial_ent(ctx, "gt", 62);
  xml_dtd_declare_trivial_ent(ctx, "amp", 38);
  xml_dtd_declare_trivial_ent(ctx, "apos", 39);
  xml_dtd_declare_trivial_ent(ctx, "quot", 34);
}

struct xml_dtd_ent *
xml_dtd_find_ent(struct xml_context *ctx, char *name)
{
  struct xml_dtd *dtd = ctx->dtd;
  if (ctx->h_resolve_entity)
    return ctx->h_resolve_entity(ctx, name);
  else if (dtd)
    {
      struct xml_dtd_ent *ent = xml_dtd_ents_find(dtd->tab_ents, name);
      return !ent ? NULL : (ent->flags & XML_DTD_ENT_DECLARED) ? ent : NULL;
    }
  else
    {
#define ENT(n, u) ent_##n = { .name = #n, .uni = u, .flags = XML_DTD_ENT_DECLARED | XML_DTD_ENT_TRIVIAL_UNI }
      static struct xml_dtd_ent ENT(lt, 60), ENT(gt, 62), ENT(amp, 38), ENT(apos, 39), ENT(quot, 34);
#undef ENT
      switch (name[0])
        {
	  case 'l':
	    if (!strcmp(name, "lt"))
	      return &ent_lt;
	    break;
	  case 'g':
	    if (!strcmp(name, "gt"))
	      return &ent_gt;
	    break;
	  case 'a':
	    if (!strcmp(name, "amp"))
	      return &ent_amp;
	    if (!strcmp(name, "apos"))
	      return &ent_apos;
	    break;
	  case 'q':
	    if (!strcmp(name, "quot"))
	      return &ent_quot;
	    break;
	}
      return NULL;
    }
}

/* Parameter entities */

static struct xml_dtd_ent *
xml_dtd_find_pent(struct xml_context *ctx, char *name)
{
  struct xml_dtd *dtd = ctx->dtd;
  struct xml_dtd_ent *ent = xml_dtd_ents_find(dtd->tab_pents, name);
  return !ent ? NULL : (ent->flags & XML_DTD_ENT_DECLARED) ? ent : NULL;
}

/* Elements */

#define HASH_PREFIX(x) xml_dtd_elems_##x
#define HASH_NODE struct xml_dtd_elem
#define HASH_KEY_STRING name
#define HASH_TABLE_DYNAMIC
#define HASH_ZERO_FILL
#define HASH_WANT_FIND
#define HASH_WANT_LOOKUP
#define HASH_GIVE_ALLOC
#define HASH_TABLE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

struct xml_dtd_elem *
xml_dtd_find_elem(struct xml_context *ctx, char *name)
{
  return ctx->dtd ? xml_dtd_elems_find(ctx->dtd->tab_elems, name) : NULL;
}

/* Element sons */

struct xml_dtd_enodes_table;

static inline uns
xml_dtd_enodes_hash(struct xml_dtd_enodes_table *tab UNUSED, struct xml_dtd_elem_node *parent, struct xml_dtd_elem *elem)
{
  return hash_pointer(parent) ^ hash_pointer(elem);
}

static inline int
xml_dtd_enodes_eq(struct xml_dtd_enodes_table *tab UNUSED, struct xml_dtd_elem_node *parent1, struct xml_dtd_elem *elem1, struct xml_dtd_elem_node *parent2, struct xml_dtd_elem *elem2)
{
  return (parent1 == parent2) && (elem1 == elem2);
}

static inline void
xml_dtd_enodes_init_key(struct xml_dtd_enodes_table *tab UNUSED, struct xml_dtd_elem_node *node, struct xml_dtd_elem_node *parent, struct xml_dtd_elem *elem)
{
  node->parent = parent;
  node->elem = elem;
}

#define HASH_PREFIX(x) xml_dtd_enodes_##x
#define HASH_NODE struct xml_dtd_elem_node
#define HASH_KEY_COMPLEX(x) x parent, x elem
#define HASH_KEY_DECL struct xml_dtd_elem_node *parent, struct xml_dtd_elem *elem
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#define HASH_TABLE_DYNAMIC
#define HASH_ZERO_FILL
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_GIVE_ALLOC
#define HASH_TABLE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

/* Element attributes */

struct xml_dtd_attrs_table;

static inline uns
xml_dtd_attrs_hash(struct xml_dtd_attrs_table *tab UNUSED, struct xml_dtd_elem *elem, char *name)
{
  return hash_pointer(elem) ^ hash_string(name);
}

static inline int
xml_dtd_attrs_eq(struct xml_dtd_attrs_table *tab UNUSED, struct xml_dtd_elem *elem1, char *name1, struct xml_dtd_elem *elem2, char *name2)
{
  return (elem1 == elem2) && !strcmp(name1, name2);
}

static inline void
xml_dtd_attrs_init_key(struct xml_dtd_attrs_table *tab UNUSED, struct xml_dtd_attr *attr, struct xml_dtd_elem *elem, char *name)
{
  attr->elem = elem;
  attr->name = name;
}

#define HASH_PREFIX(x) xml_dtd_attrs_##x
#define HASH_NODE struct xml_dtd_attr
#define HASH_ZERO_FILL
#define HASH_TABLE_DYNAMIC
#define HASH_KEY_COMPLEX(x) x elem, x name
#define HASH_KEY_DECL struct xml_dtd_elem *elem, char *name
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_GIVE_ALLOC
#define HASH_TABLE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

struct xml_dtd_attr *
xml_dtd_find_attr(struct xml_context *ctx, struct xml_dtd_elem *elem, char *name)
{
  return ctx->dtd ? xml_dtd_attrs_find(ctx->dtd->tab_attrs, elem, name) : NULL;
}

/* Enumerated attribute values */

struct xml_dtd_evals_table;

static inline uns
xml_dtd_evals_hash(struct xml_dtd_evals_table *tab UNUSED, struct xml_dtd_attr *attr, char *val)
{
  return hash_pointer(attr) ^ hash_string(val);
}

static inline int
xml_dtd_evals_eq(struct xml_dtd_evals_table *tab UNUSED, struct xml_dtd_attr *attr1, char *val1, struct xml_dtd_attr *attr2, char *val2)
{
  return (attr1 == attr2) && !strcmp(val1, val2);
}

static inline void
xml_dtd_evals_init_key(struct xml_dtd_evals_table *tab UNUSED, struct xml_dtd_eval *eval, struct xml_dtd_attr *attr, char *val)
{
  eval->attr = attr;
  eval->val = val;
}

#define HASH_PREFIX(x) xml_dtd_evals_##x
#define HASH_NODE struct xml_dtd_eval
#define HASH_TABLE_DYNAMIC
#define HASH_KEY_COMPLEX(x) x attr, x val
#define HASH_KEY_DECL struct xml_dtd_attr *attr, char *val
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_GIVE_ALLOC
#define HASH_TABLE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

/* Enumerated attribute notations */

struct xml_dtd_enotns_table;

static inline uns
xml_dtd_enotns_hash(struct xml_dtd_enotns_table *tab UNUSED, struct xml_dtd_attr *attr, struct xml_dtd_notn *notn)
{
  return hash_pointer(attr) ^ hash_pointer(notn);
}

static inline int
xml_dtd_enotns_eq(struct xml_dtd_enotns_table *tab UNUSED, struct xml_dtd_attr *attr1, struct xml_dtd_notn *notn1, struct xml_dtd_attr *attr2, struct xml_dtd_notn *notn2)
{
  return (attr1 == attr2) && (notn1 == notn2);
}

static inline void
xml_dtd_enotns_init_key(struct xml_dtd_enotns_table *tab UNUSED, struct xml_dtd_enotn *enotn, struct xml_dtd_attr *attr, struct xml_dtd_notn *notn)
{
  enotn->attr = attr;
  enotn->notn = notn;
}

#define HASH_PREFIX(x) xml_dtd_enotns_##x
#define HASH_NODE struct xml_dtd_enotn
#define HASH_TABLE_DYNAMIC
#define HASH_KEY_COMPLEX(x) x attr, x notn
#define HASH_KEY_DECL struct xml_dtd_attr *attr, struct xml_dtd_notn *notn
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_GIVE_ALLOC
#define HASH_TABLE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

/* DTD initialization/cleanup */

void
xml_dtd_init(struct xml_context *ctx)
{
  if (ctx->dtd)
    return;
  struct mempool *pool = mp_new(4096);
  struct xml_dtd *dtd = ctx->dtd = mp_alloc_zero(pool, sizeof(*ctx->dtd));
  dtd->pool = pool;
  xml_dtd_ents_init(dtd->tab_ents = xml_hash_new(pool, sizeof(struct xml_dtd_ents_table)));
  xml_dtd_ents_init(dtd->tab_pents = xml_hash_new(pool, sizeof(struct xml_dtd_ents_table)));
  xml_dtd_notns_init(dtd->tab_notns = xml_hash_new(pool, sizeof(struct xml_dtd_notns_table)));
  xml_dtd_elems_init(dtd->tab_elems = xml_hash_new(pool, sizeof(struct xml_dtd_elems_table)));
  xml_dtd_enodes_init(dtd->tab_enodes = xml_hash_new(pool, sizeof(struct xml_dtd_enodes_table)));
  xml_dtd_attrs_init(dtd->tab_attrs = xml_hash_new(pool, sizeof(struct xml_dtd_attrs_table)));
  xml_dtd_evals_init(dtd->tab_evals = xml_hash_new(pool, sizeof(struct xml_dtd_evals_table)));
  xml_dtd_enotns_init(dtd->tab_enotns = xml_hash_new(pool, sizeof(struct xml_dtd_enotns_table)));
  xml_dtd_declare_default_ents(ctx);
}

void
xml_dtd_cleanup(struct xml_context *ctx)
{
  if (!ctx->dtd)
    return;
  mp_delete(ctx->dtd->pool);
  ctx->dtd = NULL;
}

void
xml_dtd_finish(struct xml_context *ctx)
{
  if (!ctx->dtd)
    return;
  // FIXME: validity checks
}

/*** Parsing functions ***/

/* References to parameter entities */

void
xml_parse_pe_ref(struct xml_context *ctx)
{
  /* PEReference ::= '%' Name ';'
   * Already parsed: '%' */
  struct mempool_state state;
  mp_save(ctx->stack, &state);
  char *name = xml_parse_name(ctx, ctx->stack);
  xml_parse_char(ctx, ';');
  struct xml_dtd_ent *ent = xml_dtd_find_pent(ctx, name);
  if (!ent)
    xml_error(ctx, "Unknown entity %%%s;", name);
  else
    {
      TRACE(ctx, "Pushed entity %%%s;", name);
      mp_restore(ctx->stack, &state);
      xml_dec(ctx);
      xml_push_entity(ctx, ent);
      return;
    }
  mp_restore(ctx->stack, &state);
  xml_dec(ctx);
}

static void
xml_parse_dtd_pe(struct xml_context *ctx)
{
  do
    {
      xml_skip_char(ctx);
      xml_inc(ctx);
      while (xml_peek_cat(ctx) & XML_CHAR_WHITE)
	xml_skip_char(ctx);
      xml_parse_pe_ref(ctx);
    }
  while (xml_peek_char(ctx) != '%');
}

static inline uns
xml_parse_dtd_white(struct xml_context *ctx, uns mandatory)
{
  /* Whitespace or parameter entity */
  uns cnt = 0;
  while (xml_peek_cat(ctx) & XML_CHAR_WHITE)
    {
      xml_skip_char(ctx);
      cnt = 1;
    }
  if (xml_peek_char(ctx) == '%')
    {
      xml_parse_dtd_pe(ctx);
      return 1;
    }
  else if (unlikely(mandatory && !cnt))
    xml_fatal_expected_white(ctx);
  return cnt;
}

static void
xml_dtd_parse_external_id(struct xml_context *ctx, struct xml_ext_id *eid, uns allow_public)
{
  struct xml_dtd *dtd = ctx->dtd;
  bzero(eid, sizeof(*eid));
  uns c = xml_peek_char(ctx);
  if (c == 'S')
    {
      xml_parse_seq(ctx, "SYSTEM");
      xml_parse_dtd_white(ctx, 1);
      eid->system_id = xml_parse_system_literal(ctx, dtd->pool);
    }
  else if (c == 'P')
    {
      xml_parse_seq(ctx, "PUBLIC");
      xml_parse_dtd_white(ctx, 1);
      eid->public_id = xml_parse_pubid_literal(ctx, dtd->pool);
      if (xml_parse_dtd_white(ctx, 0)) // FIXME
	if ((c = xml_peek_char(ctx)) == '\'' || c == '"' || !allow_public)
	  eid->system_id = xml_parse_system_literal(ctx, dtd->pool);
    }
  else
    xml_fatal(ctx, "Expected an external ID");
}

/* DTD: <!NOTATION ...> */

void
xml_parse_notation_decl(struct xml_context *ctx)
{
  /* NotationDecl ::= '<!NOTATION' S Name S (ExternalID | PublicID) S? '>'
   * Already parsed: '<!NOTATION' */
  TRACE(ctx, "parse_notation_decl");
  struct xml_dtd *dtd = ctx->dtd;
  xml_parse_dtd_white(ctx, 1);

  struct xml_dtd_notn *notn = xml_dtd_notns_lookup(dtd->tab_notns, xml_parse_name(ctx, dtd->pool));
  xml_parse_dtd_white(ctx, 1);
  struct xml_ext_id eid;
  xml_dtd_parse_external_id(ctx, &eid, 1);
  xml_parse_dtd_white(ctx, 0);
  xml_parse_char(ctx, '>');

  if (notn->flags & XML_DTD_NOTN_DECLARED)
    xml_warn(ctx, "Notation %s already declared", notn->name);
  else
    {
      notn->flags = XML_DTD_NOTN_DECLARED;
      notn->eid = eid;
      slist_add_tail(&dtd->notns, &notn->n);
    }
  xml_dec(ctx);
}

/* DTD: <!ENTITY ...> */

void
xml_parse_entity_decl(struct xml_context *ctx)
{
  /* Already parsed: '<!ENTITY' */
  TRACE(ctx, "parse_entity_decl");
  struct xml_dtd *dtd = ctx->dtd;
  xml_parse_dtd_white(ctx, 1);

  uns flags = (xml_get_char(ctx) == '%') ? XML_DTD_ENT_PARAMETER : 0;
  if (flags)
    xml_parse_dtd_white(ctx, 1);
  else
    xml_unget_char(ctx);

  struct xml_dtd_ent *ent = xml_dtd_ents_lookup(flags ? dtd->tab_pents : dtd->tab_ents, xml_parse_name(ctx, dtd->pool));
  slist *list = flags ? &dtd->pents : &dtd->ents;
  xml_parse_dtd_white(ctx, 1);
  if (ent->flags & XML_DTD_ENT_DECLARED)
    {
       xml_fatal(ctx, "Entity &%s; already declared, skipping not implemented", ent->name);
       // FIXME: should be only warning
    }

  uns c, sep = xml_get_char(ctx);
  if (sep == '\'' || sep == '"')
    {
      /* Internal entity:
       * EntityValue ::= '"' ([^%&"] | PEReference | Reference)* '"' | "'" ([^%&'] | PEReference | Reference)* "'" */
      char *p = mp_start_noalign(dtd->pool, 1);
      while (1)
        {
	  if ((c = xml_get_char(ctx)) == sep)
	    break;
	  if (c == '%')
	    {
	      // FIXME
	      ASSERT(0);
	      //xml_parse_parameter_ref(ctx);
	      continue;
	    }
	  if (c == '&')
	    {
	      xml_inc(ctx);
	      if (xml_peek_char(ctx) != '#')
	        {
	          /* Bypass references to general entities */
	          struct mempool_state state;
	          mp_save(ctx->stack, &state);
	          char *n = xml_parse_name(ctx, ctx->stack);
	          xml_parse_char(ctx, ';');
		  xml_dec(ctx);
		  uns l = strlen(n);
		  p = mp_spread(dtd->pool, p, 3 + l);
		  *p++ = '&';
		  memcpy(p, n, l);
		  *p++ = ';';;
		  mp_restore(ctx->stack, &state);
		  continue;
	        }
	      else
	        {
		  xml_skip_char(ctx);
	          c = xml_parse_char_ref(ctx);
		}
	    }
	  p = mp_spread(dtd->pool, p, 5);
	  p = utf8_32_put(p, c);
	}
      *p = 0;
      ent->len = p - (char *)mp_ptr(dtd->pool);
      ent->text = mp_end(dtd->pool, p + 1);
      slist_add_tail(list, &ent->n);
      ent->flags = flags | XML_DTD_ENT_DECLARED;
    }
  else
    {
      /* External entity */
      struct xml_ext_id eid;
      struct xml_dtd_notn *notn = NULL;
      xml_dtd_parse_external_id(ctx, &eid, 0);
      if (!xml_parse_dtd_white(ctx, 0) || !flags)
	xml_parse_char(ctx, '>');
      else if (xml_get_char(ctx) != '>')
        {
	  /* General external unparsed entity */
	  flags |= XML_DTD_ENT_UNPARSED;
	  xml_parse_seq(ctx, "NDATA");
	  xml_parse_dtd_white(ctx, 1);
	  notn = xml_dtd_notns_lookup(dtd->tab_notns, xml_parse_name(ctx, dtd->pool));
	}
      slist_add_tail(list, &ent->n);
      ent->flags = flags | XML_DTD_ENT_DECLARED | XML_DTD_ENT_EXTERNAL;
      ent->eid = eid;
      ent->notn = notn;
    }
  xml_parse_dtd_white(ctx, 0);
  xml_parse_char(ctx, '>');
  xml_dec(ctx);
}

/* DTD: <!ELEMENT ...> */

void
xml_parse_element_decl(struct xml_context *ctx)
{
  /* Elementdecl ::= '<!ELEMENT' S  Name  S  contentspec  S? '>'
   * Already parsed: '<!ELEMENT' */
  struct xml_dtd *dtd = ctx->dtd;
  xml_parse_dtd_white(ctx, 1);
  char *name = xml_parse_name(ctx, dtd->pool);
  xml_parse_dtd_white(ctx, 1);
  struct xml_dtd_elem *elem = xml_dtd_elems_lookup(dtd->tab_elems, name);
  if (elem->flags & XML_DTD_ELEM_DECLARED)
    xml_fatal(ctx, "Element <%s> already declared", name);

  /* contentspec ::= 'EMPTY' | 'ANY' | Mixed | children */
  uns c = xml_peek_char(ctx);
  if (c == 'E')
    {
      xml_parse_seq(ctx, "EMPTY");
      elem->type = XML_DTD_ELEM_EMPTY;
    }
  else if (c == 'A')
    {
      xml_parse_seq(ctx, "ANY");
      elem->type = XML_DTD_ELEM_ANY;
    }
  else if (c == '(')
    {
      xml_skip_char(ctx);
      xml_inc(ctx);
      xml_parse_dtd_white(ctx, 0);
      struct xml_dtd_elem_node *parent = elem->node = mp_alloc_zero(dtd->pool, sizeof(*parent));
      if (xml_peek_char(ctx) == '#')
        {
	  /* Mixed ::= '(' S? '#PCDATA' (S? '|' S? Name)* S? ')*' | '(' S? '#PCDATA' S? ')' */
	  xml_skip_char(ctx);
	  xml_parse_seq(ctx, "PCDATA");
	  elem->type = XML_DTD_ELEM_MIXED;
          parent->type = XML_DTD_ELEM_PCDATA;
	  while (1)
	    {
	      xml_parse_dtd_white(ctx, 0);
	      if ((c = xml_get_char(ctx)) == ')')
		break;
	      else if (c != '|')
		xml_fatal_expected(ctx, ')');
	      xml_parse_dtd_white(ctx, 0);
	      struct xml_dtd_elem *son_elem = xml_dtd_elems_lookup(dtd->tab_elems, xml_parse_name(ctx, dtd->pool));
	      if (xml_dtd_enodes_find(dtd->tab_enodes, parent, son_elem))
		xml_error(ctx, "Duplicate content '%s'", son_elem->name);
	      else
	        {
		  struct xml_dtd_elem_node *son = xml_dtd_enodes_new(dtd->tab_enodes, parent, son_elem);
		  slist_add_tail(&parent->sons, &son->n);
		}
	    }
	  xml_dec(ctx);
	  if (xml_peek_char(ctx) == '*')
	    {
	      xml_skip_char(ctx);
	      parent->occur = XML_DTD_ELEM_OCCUR_MULT;
	    }
	  else if (!slist_head(&parent->sons))
	    parent->occur = XML_DTD_ELEM_OCCUR_ONCE;
	  else
	    xml_fatal_expected(ctx, '*');
	}
      else
        {
	  /* children ::= (choice | seq) ('?' | '*' | '+')?
	   * cp ::= (Name | choice | seq) ('?' | '*' | '+')?
	   * choice ::= '(' S? cp ( S? '|' S? cp )+ S? ')'
	   * seq ::= '(' S? cp ( S? ',' S? cp )* S? ')' */

	  elem->type = XML_DTD_ELEM_CHILDREN;
	  parent->type = XML_DTD_ELEM_PCDATA;
	  uns c;
	  goto first;

	  while (1)
	    {
	      /* After name */
	      xml_parse_dtd_white(ctx, 0);
	      if ((c = xml_get_char(ctx)) ==  ')')
	        {
		  xml_dec(ctx);
		  if (parent->type == XML_DTD_ELEM_PCDATA)
		    parent->type = XML_DTD_ELEM_SEQ;
		  if ((c = xml_get_char(ctx)) == '?')
		    parent->occur = XML_DTD_ELEM_OCCUR_OPT;
		  else if (c == '*')
		    parent->occur = XML_DTD_ELEM_OCCUR_MULT;
		  else if (c == '+')
		    parent->occur = XML_DTD_ELEM_OCCUR_PLUS;
		  else
		    {
		      xml_unget_char(ctx);
		      parent->occur = XML_DTD_ELEM_OCCUR_ONCE;
		    }
		  if (!parent->parent)
		    break;
		  parent = parent->parent;
		  continue;
		}
	      else if (c == '|')
	        {
		  if (parent->type == XML_DTD_ELEM_PCDATA)
		    parent->type = XML_DTD_ELEM_OR;
		  else if (parent->type != XML_DTD_ELEM_OR)
		    xml_fatal(ctx, "Mixed operators in the list of element children");
		}
	      else if (c == ',')
	        {
		  if (parent->type == XML_DTD_ELEM_PCDATA)
		    parent->type = XML_DTD_ELEM_SEQ;
		  else if (parent->type != XML_DTD_ELEM_SEQ)
		    xml_fatal(ctx, "Mixed operators in the list of element children");
		}
	      else if (c == '(')
	        {
		  xml_inc(ctx);
		  struct xml_dtd_elem_node *son = mp_alloc_zero(dtd->pool, sizeof(*son));
		  son->parent = parent;
		  slist_add_tail(&parent->sons, &son->n);
		  parent = son->parent;
		  son->type = XML_DTD_ELEM_MIXED;
		}
	      else
	        xml_unget_char(ctx);

	      /* Before name */
	      xml_parse_dtd_white(ctx, 0);
first:;
	      struct xml_dtd_elem *son_elem = xml_dtd_elems_lookup(dtd->tab_elems, xml_parse_name(ctx, dtd->pool));
	      // FIXME: duplicates, occurance
	      //struct xml_dtd_elem_node *son = xml_dtd_enodes_new(dtd->tab_enodes, parent, son_elem);
	      struct xml_dtd_elem_node *son = mp_alloc_zero(dtd->pool, sizeof(*son));
	      son->parent = parent;
	      son->elem = son_elem;
	      slist_add_tail(&parent->sons, &son->n);
	    }
	}
    }
  else
    xml_fatal(ctx, "Expected element content specification");

  xml_parse_dtd_white(ctx, 0);
  xml_parse_char(ctx, '>');
  xml_dec(ctx);
}

void
xml_parse_attr_list_decl(struct xml_context *ctx)
{
  /* AttlistDecl ::= '<!ATTLIST' S Name AttDef* S? '>'
   * AttDef ::= S Name S AttType S DefaultDecl
   * Already parsed: '<!ATTLIST' */
  struct xml_dtd *dtd = ctx->dtd;
  xml_parse_dtd_white(ctx, 1);
  struct xml_dtd_elem *elem = xml_dtd_elems_lookup(ctx->dtd->tab_elems, xml_parse_name(ctx, dtd->pool));

  while (xml_parse_dtd_white(ctx, 0) && xml_peek_char(ctx) != '>')
    {
      char *name = xml_parse_name(ctx, dtd->pool);
      struct xml_dtd_attr *attr = xml_dtd_attrs_find(dtd->tab_attrs, elem, name);
      uns ignored = 0;
      if (attr)
        {
	  xml_warn(ctx, "Duplicate attribute definition");
	  ignored++;
	}
      else
	attr = xml_dtd_attrs_new(ctx->dtd->tab_attrs, elem, name);
      xml_parse_dtd_white(ctx, 1);
      if (xml_peek_char(ctx) == '(')
        {
	  xml_skip_char(ctx); // FIXME: xml_inc/dec ?
	  if (!ignored)
	    attr->type = XML_ATTR_ENUM;
	  do
	    {
	      xml_parse_dtd_white(ctx, 0);
	      char *value = xml_parse_nmtoken(ctx, dtd->pool);
	      if (!ignored)
		if (xml_dtd_evals_find(ctx->dtd->tab_evals, attr, value))
		  xml_error(ctx, "Duplicate enumeration value");
	        else
		  xml_dtd_evals_new(ctx->dtd->tab_evals, attr, value);
	      xml_parse_dtd_white(ctx, 0);
	    }
	  while (xml_get_char(ctx) == '|');
	  xml_unget_char(ctx);
	  xml_parse_char(ctx, ')');
	}
      else
        {
	  char *type = xml_parse_name(ctx, dtd->pool);
	  enum xml_dtd_attribute_type t = XML_ATTR_CDATA;
	  if (!strcmp(type, "CDATA"))
	    t = XML_ATTR_CDATA;
	  else if (!strcmp(type, "ID"))
	    t = XML_ATTR_ID;
	  else if (!strcmp(type, "IDREF"))
	    t = XML_ATTR_IDREF;
	  else if (!strcmp(type, "IDREFS"))
	    t = XML_ATTR_IDREFS;
	  else if (!strcmp(type, "ENTITY"))
	    t = XML_ATTR_ENTITY;
	  else if (!strcmp(type, "ENTITIES"))
	    t = XML_ATTR_ENTITIES;
	  else if (!strcmp(type, "NMTOKEN"))
	    t = XML_ATTR_NMTOKEN;
	  else if (!strcmp(type, "NMTOKENS"))
	    t = XML_ATTR_NMTOKENS;
	  else if (!strcmp(type, "NOTATION"))
	    {
	      if (elem->type == XML_DTD_ELEM_EMPTY)
		xml_fatal(ctx, "Empty element must not have notation attribute");
	      // FIXME: An element type MUST NOT have more than one NOTATION attribute specified.
	      t = XML_ATTR_NOTATION;
	      xml_parse_dtd_white(ctx, 1);
	      xml_parse_char(ctx, '(');
	      do
	        {
		  xml_parse_dtd_white(ctx, 0);
		  struct xml_dtd_notn *n = xml_dtd_notns_lookup(ctx->dtd->tab_notns, xml_parse_name(ctx, dtd->pool));
		  if (!ignored)
		    if (xml_dtd_enotns_find(ctx->dtd->tab_enotns, attr, n))
		      xml_error(ctx, "Duplicate enumerated notation");
		    else
		      xml_dtd_enotns_new(ctx->dtd->tab_enotns, attr, n);
		  xml_parse_dtd_white(ctx, 0);
		}
	      while (xml_get_char(ctx) == '|');
	      xml_unget_char(ctx);
	      xml_parse_char(ctx, ')');
	    }
	  else
	    xml_fatal(ctx, "Unknown attribute type");
	  if (!ignored)
	    attr->type = t;
	}
      xml_parse_dtd_white(ctx, 1);
      enum xml_dtd_attribute_default def = XML_ATTR_NONE;
      if (xml_get_char(ctx) == '#')
	switch (xml_peek_char(ctx))
          {
	    case 'R':
	      xml_parse_seq(ctx, "REQUIRED");
	      def = XML_ATTR_REQUIRED;
	      break;
	    case 'I':
	      xml_parse_seq(ctx, "IMPLIED");
	      def = XML_ATTR_IMPLIED;
	      break;
	    case 'F':
	      xml_parse_seq(ctx, "FIXED");
	      def = XML_ATTR_FIXED;
	      xml_parse_dtd_white(ctx, 1);
	      break;
	    default:
	      xml_fatal(ctx, "Expected a modifier for default attribute value");
	  }
      else
	xml_unget_char(ctx);
      if (def != XML_ATTR_REQUIRED && def != XML_ATTR_IMPLIED)
        {
	  char *v = xml_parse_attr_value(ctx, attr);
	  if (!ignored)
	    attr->default_value = v;
	}
      if (!ignored)
	attr->default_mode = def;
    }
  xml_skip_char(ctx);
  xml_dec(ctx);
}

void
xml_skip_internal_subset(struct xml_context *ctx)
{
  TRACE(ctx, "skip_internal_subset");
  /* AlreadyParsed: '[' */
  uns c;
  while ((c = xml_get_char(ctx)) != ']')
    {
      if (c != '<')
	continue;
      if ((c = xml_get_char(ctx)) == '?')
        {
          xml_inc(ctx);
	  xml_skip_pi(ctx);
	}
      else if (c != '!')
	xml_dec(ctx);
      else if (xml_get_char(ctx) == '-')
        {
	  xml_inc(ctx);
	  xml_skip_comment(ctx);
	}
      else
	while ((c = xml_get_char(ctx)) != '>')
	  if (c == '\'' || c == '"')
	    while (xml_get_char(ctx) != c);
    }
  xml_dec(ctx);
}
