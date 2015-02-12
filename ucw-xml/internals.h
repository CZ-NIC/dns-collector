/*
 *	UCW Library -- A simple XML parser
 *
 *	(c) 2007--2008 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_XML_INTERNALS_H
#define _UCW_XML_INTERNALS_H

#include <ucw-xml/xml.h>
#include <ucw-xml/dtd.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define xml_attrs_table_cleanup ucw_xml_attrs_table_cleanup
#define xml_attrs_table_init ucw_xml_attrs_table_init
#define xml_fatal_expected ucw_xml_fatal_expected
#define xml_fatal_expected_quot ucw_xml_fatal_expected_quot
#define xml_fatal_expected_white ucw_xml_fatal_expected_white
#define xml_fatal_nested ucw_xml_fatal_nested
#define xml_hash_new ucw_xml_hash_new
#define xml_parse_attr_list_decl ucw_xml_parse_attr_list_decl
#define xml_parse_attr_value ucw_xml_parse_attr_value
#define xml_parse_char_ref ucw_xml_parse_char_ref
#define xml_parse_element_decl ucw_xml_parse_element_decl
#define xml_parse_entity_decl ucw_xml_parse_entity_decl
#define xml_parse_eq ucw_xml_parse_eq
#define xml_parse_name ucw_xml_parse_name
#define xml_parse_nmtoken ucw_xml_parse_nmtoken
#define xml_parse_notation_decl ucw_xml_parse_notation_decl
#define xml_parse_pe_ref ucw_xml_parse_pe_ref
#define xml_parse_pubid_literal ucw_xml_parse_pubid_literal
#define xml_parse_system_literal ucw_xml_parse_system_literal
#define xml_pop_comment ucw_xml_pop_comment
#define xml_pop_pi ucw_xml_pop_pi
#define xml_push_comment ucw_xml_push_comment
#define xml_push_entity ucw_xml_push_entity
#define xml_push_pi ucw_xml_push_pi
#define xml_push_source ucw_xml_push_source
#define xml_refill ucw_xml_refill
#define xml_skip_comment ucw_xml_skip_comment
#define xml_skip_internal_subset ucw_xml_skip_internal_subset
#define xml_skip_name ucw_xml_skip_name
#define xml_skip_pi ucw_xml_skip_pi
#define xml_sources_cleanup ucw_xml_sources_cleanup
#define xml_spout_chars ucw_xml_spout_chars
#define xml_throw ucw_xml_throw
#define xml_validate_attr ucw_xml_validate_attr
#endif

/*** Debugging ***/

#ifdef LOCAL_DEBUG
#define TRACE(c, f, p...) do { DBG("XML %u: " f, xml_row(c), ##p); } while(0)
#else
#define TRACE(c, f, p...) do {} while(0)
#endif

/*** Error handling ***/

void NONRET xml_throw(struct xml_context *ctx);

/*** Memory management ***/

struct xml_stack {
  struct xml_stack *next;
  struct mempool_state state;
  uint flags;
};

static inline void *xml_do_push(struct xml_context *ctx, uint size)
{
  /* Saves ctx->stack and ctx->flags state */
  struct mempool_state state;
  mp_save(ctx->stack, &state);
  struct xml_stack *s = mp_alloc(ctx->stack, size);
  s->state = state;
  s->flags = ctx->flags;
  s->next = ctx->stack_list;
  ctx->stack_list = s;
  return s;
}

static inline void xml_do_pop(struct xml_context *ctx, struct xml_stack *s)
{
  /* Restore ctx->stack and ctx->flags state */
  ctx->stack_list = s->next;
  ctx->flags = s->flags;
  mp_restore(ctx->stack, &s->state);
}

static inline void xml_push(struct xml_context *ctx)
{
  TRACE(ctx, "push");
  xml_do_push(ctx, sizeof(struct xml_stack));
}

static inline void xml_pop(struct xml_context *ctx)
{
  TRACE(ctx, "pop");
  ASSERT(ctx->stack_list);
  xml_do_pop(ctx, ctx->stack_list);
}

struct xml_dom_stack {
  struct xml_stack stack;
  struct mempool_state state;
};

static inline struct xml_node *xml_push_dom(struct xml_context *ctx, struct mempool_state *state)
{
  /* Create a new DOM node */
  TRACE(ctx, "push_dom");
  struct xml_dom_stack *s = xml_do_push(ctx, sizeof(*s));
  if (state)
    s->state = *state;
  else
    mp_save(ctx->pool, &s->state);
  struct xml_node *n = mp_alloc(ctx->pool, sizeof(*n));
  n->user = NULL;
  if (n->parent = ctx->node)
    clist_add_tail(&n->parent->sons, &n->n);
  return ctx->node = n;
}

static inline void xml_pop_dom(struct xml_context *ctx, uint free)
{
  /* Leave DOM subtree */
  TRACE(ctx, "pop_dom");
  ASSERT(ctx->node);
  struct xml_node *p = ctx->node->parent;
  struct xml_dom_stack *s = (void *)ctx->stack_list;
  if (free)
    {
      /* See xml_pop_element() for cleanup of attribute hash table */
      if (p)
        clist_remove(&ctx->node->n);
      mp_restore(ctx->pool, &s->state);
    }
  ctx->node = p;
  xml_do_pop(ctx, &s->stack);
}

#define XML_HASH_HDR_SIZE ALIGN_TO(sizeof(void *), CPU_STRUCT_ALIGN)
#define XML_HASH_GIVE_ALLOC struct HASH_PREFIX(table); \
  static inline void *HASH_PREFIX(alloc)(struct HASH_PREFIX(table) *t, uint size) \
  { return mp_alloc(*(void **)((void *)t - XML_HASH_HDR_SIZE), size); } \
  static inline void HASH_PREFIX(free)(struct HASH_PREFIX(table) *t UNUSED, void *p UNUSED) {}

void *xml_hash_new(struct mempool *pool, uint size);

void xml_spout_chars(struct fastbuf *fb);

/*** Reading of document/external entities ***/

void NONRET xml_fatal_nested(struct xml_context *ctx);

static inline void xml_inc(struct xml_context *ctx)
{
  /* Called after the first character of a block */
  TRACE(ctx, "inc");
  ctx->depth++;
}

static inline void xml_dec(struct xml_context *ctx)
{
  /* Called after the last character of a block */
  TRACE(ctx, "dec");
  if (unlikely(!ctx->depth--))
    xml_fatal_nested(ctx);
}

#include "obj/ucw-xml/unicat.h"

static inline uint xml_char_cat(uint c)
{
  if (c < 0x10000)
    return 1U << ucw_xml_char_tab1[(c & 0xff) + ucw_xml_char_tab2[c >> 8]];
  else if (likely(c < 0x110000))
    return 1U << ucw_xml_char_tab3[c >> 16];
  else
    return 1;
}

static inline uint xml_ascii_cat(uint c)
{
  return ucw_xml_char_tab1[c];
}

struct xml_source *xml_push_source(struct xml_context *ctx);
void xml_push_entity(struct xml_context *ctx, struct xml_dtd_entity *ent);

void xml_refill(struct xml_context *ctx);

static inline uint xml_peek_char(struct xml_context *ctx)
{
  if (ctx->bptr == ctx->bstop)
    xml_refill(ctx);
  return ctx->bptr[0];
}

static inline uint xml_peek_cat(struct xml_context *ctx)
{
  if (ctx->bptr == ctx->bstop)
    xml_refill(ctx);
  return ctx->bptr[1];
}

static inline uint xml_get_char(struct xml_context *ctx)
{
  uint c = xml_peek_char(ctx);
  ctx->bptr += 2;
  return c;
}

static inline uint xml_get_cat(struct xml_context *ctx)
{
  uint c = xml_peek_cat(ctx);
  ctx->bptr += 2;
  return c;
}

static inline uint xml_last_char(struct xml_context *ctx)
{
  return ctx->bptr[-2];
}

static inline uint xml_last_cat(struct xml_context *ctx)
{
  return ctx->bptr[-1];
}

static inline uint xml_skip_char(struct xml_context *ctx)
{
  uint c = ctx->bptr[0];
  ctx->bptr += 2;
  return c;
}

static inline uint xml_unget_char(struct xml_context *ctx)
{
  return *(ctx->bptr -= 2);
}

void xml_sources_cleanup(struct xml_context *ctx);

/*** Parsing ***/

void NONRET xml_fatal_expected(struct xml_context *ctx, uint c);
void NONRET xml_fatal_expected_white(struct xml_context *ctx);
void NONRET xml_fatal_expected_quot(struct xml_context *ctx);

static inline uint xml_parse_white(struct xml_context *ctx, uint mandatory)
{
  /* mandatory=1 -> S ::= (#x20 | #x9 | #xD | #xA)+
   * mandatory=0 -> S? */
  uint cnt = 0;
  while (xml_peek_cat(ctx) & XML_CHAR_WHITE)
    {
      xml_skip_char(ctx);
      cnt++;
    }
  if (unlikely(mandatory && !cnt))
    xml_fatal_expected_white(ctx);
  return cnt;
}

static inline void xml_parse_char(struct xml_context *ctx, uint c)
{
  /* Consumes a given Unicode character */
  if (unlikely(c != xml_get_char(ctx)))
    xml_fatal_expected(ctx, c);
}

static inline void xml_parse_seq(struct xml_context *ctx, const char *seq)
{
  /* Consumes a given sequence of ASCII characters */
  while (*seq)
    xml_parse_char(ctx, *seq++);
}

void xml_parse_eq(struct xml_context *ctx);

static inline uint xml_parse_quote(struct xml_context *ctx)
{
  /* "'" | '"' */
  uint c = xml_get_char(ctx);
  if (unlikely(c != '\'' && c != '\"'))
    xml_fatal_expected_quot(ctx);
  return c;
}

char *xml_parse_name(struct xml_context *ctx, struct mempool *pool);
void xml_skip_name(struct xml_context *ctx);
char *xml_parse_nmtoken(struct xml_context *ctx, struct mempool *pool);

char *xml_parse_system_literal(struct xml_context *ctx, struct mempool *pool);
char *xml_parse_pubid_literal(struct xml_context *ctx, struct mempool *pool);

uint xml_parse_char_ref(struct xml_context *ctx);
void xml_parse_pe_ref(struct xml_context *ctx);

char *xml_parse_attr_value(struct xml_context *ctx, struct xml_dtd_attr *attr);

void xml_skip_internal_subset(struct xml_context *ctx);
void xml_parse_notation_decl(struct xml_context *ctx);
void xml_parse_entity_decl(struct xml_context *ctx);
void xml_parse_element_decl(struct xml_context *ctx);
void xml_parse_attr_list_decl(struct xml_context *ctx);

void xml_push_comment(struct xml_context *ctx);
void xml_pop_comment(struct xml_context *ctx);
void xml_skip_comment(struct xml_context *ctx);

void xml_push_pi(struct xml_context *ctx);
void xml_pop_pi(struct xml_context *ctx);
void xml_skip_pi(struct xml_context *ctx);

void xml_attrs_table_init(struct xml_context *ctx);
void xml_attrs_table_cleanup(struct xml_context *ctx);

void xml_validate_attr(struct xml_context *ctx, struct xml_dtd_attr *dtd, char *value);

#endif
