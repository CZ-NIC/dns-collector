/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007--2008 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_XML_INTERNALS_H
#define _SHERLOCK_XML_INTERNALS_H

#include <xml/xml.h>
#include <xml/dtd.h>

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
  uns flags;
};

static inline void *
xml_do_push(struct xml_context *ctx, uns size)
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

static inline void
xml_do_pop(struct xml_context *ctx, struct xml_stack *s)
{
  /* Restore ctx->stack and ctx->flags state */
  ctx->stack_list = s->next;
  ctx->flags = s->flags;
  mp_restore(ctx->stack, &s->state);
}

static inline void
xml_push(struct xml_context *ctx)
{
  TRACE(ctx, "push");
  xml_do_push(ctx, sizeof(struct xml_stack));
}

static inline void
xml_pop(struct xml_context *ctx)
{
  TRACE(ctx, "pop");
  ASSERT(ctx->stack_list);
  xml_do_pop(ctx, ctx->stack_list);
}

struct xml_dom_stack {
  struct xml_stack stack;
  struct mempool_state state;
};

static inline struct xml_node *
xml_push_dom(struct xml_context *ctx, struct mempool_state *state)
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

static inline void
xml_pop_dom(struct xml_context *ctx, uns free)
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
  static inline void *HASH_PREFIX(alloc)(struct HASH_PREFIX(table) *t, uns size) \
  { return mp_alloc(*(void **)((void *)t - XML_HASH_HDR_SIZE), size); } \
  static inline void HASH_PREFIX(free)(struct HASH_PREFIX(table) *t UNUSED, void *p UNUSED) {}

void *xml_hash_new(struct mempool *pool, uns size);

void xml_spout_chars(struct fastbuf *fb);

/*** Reading of document/external entities ***/

void NONRET xml_fatal_nested(struct xml_context *ctx);

static inline void
xml_inc(struct xml_context *ctx)
{
  /* Called after the first character of a block */
  TRACE(ctx, "inc");
  ctx->depth++;
}

static inline void
xml_dec(struct xml_context *ctx)
{
  /* Called after the last character of a block */
  TRACE(ctx, "dec");
  if (unlikely(!ctx->depth--))
    xml_fatal_nested(ctx);
}

#include "obj/xml/unicat.h"

static inline uns
xml_char_cat(uns c)
{
  if (c < 0x10000)
    return 1U << xml_char_tab1[(c & 0xff) + xml_char_tab2[c >> 8]];
  else if (likely(c < 0x110000))
    return 1U << xml_char_tab3[c >> 16];
  else
    return 1;
}

static inline uns
xml_ascii_cat(uns c)
{
  return xml_char_tab1[c];
}

struct xml_source *xml_push_source(struct xml_context *ctx);
void xml_push_entity(struct xml_context *ctx, struct xml_dtd_entity *ent);

void xml_refill(struct xml_context *ctx);

static inline uns
xml_peek_char(struct xml_context *ctx)
{
  if (ctx->bptr == ctx->bstop)
    xml_refill(ctx);
  return ctx->bptr[0];
}

static inline uns
xml_peek_cat(struct xml_context *ctx)
{
  if (ctx->bptr == ctx->bstop)
    xml_refill(ctx);
  return ctx->bptr[1];
}

static inline uns
xml_get_char(struct xml_context *ctx)
{
  uns c = xml_peek_char(ctx);
  ctx->bptr += 2;
  return c;
}

static inline uns
xml_get_cat(struct xml_context *ctx)
{
  uns c = xml_peek_cat(ctx);
  ctx->bptr += 2;
  return c;
}

static inline uns
xml_last_char(struct xml_context *ctx)
{
  return ctx->bptr[-2];
}

static inline uns
xml_last_cat(struct xml_context *ctx)
{
  return ctx->bptr[-1];
}

static inline uns
xml_skip_char(struct xml_context *ctx)
{
  uns c = ctx->bptr[0];
  ctx->bptr += 2;
  return c;
}

static inline uns
xml_unget_char(struct xml_context *ctx)
{
  return *(ctx->bptr -= 2);
}

void xml_sources_cleanup(struct xml_context *ctx);

/*** Parsing ***/

void NONRET xml_fatal_expected(struct xml_context *ctx, uns c);
void NONRET xml_fatal_expected_white(struct xml_context *ctx);
void NONRET xml_fatal_expected_quot(struct xml_context *ctx);

static inline uns
xml_parse_white(struct xml_context *ctx, uns mandatory)
{
  /* mandatory=1 -> S ::= (#x20 | #x9 | #xD | #xA)+
   * mandatory=0 -> S? */
  uns cnt = 0;
  while (xml_peek_cat(ctx) & XML_CHAR_WHITE)
    {
      xml_skip_char(ctx);
      cnt++;
    }
  if (unlikely(mandatory && !cnt))
    xml_fatal_expected_white(ctx);
  return cnt;
}

static inline void
xml_parse_char(struct xml_context *ctx, uns c)
{
  /* Consumes a given Unicode character */
  if (unlikely(c != xml_get_char(ctx)))
    xml_fatal_expected(ctx, c);
}

static inline void
xml_parse_seq(struct xml_context *ctx, const char *seq)
{
  /* Consumes a given sequence of ASCII characters */
  while (*seq)
    xml_parse_char(ctx, *seq++);
}

void xml_parse_eq(struct xml_context *ctx);

static inline uns
xml_parse_quote(struct xml_context *ctx)
{
  /* "'" | '"' */
  uns c = xml_get_char(ctx);
  if (unlikely(c != '\'' && c != '\"'))
    xml_fatal_expected_quot(ctx);
  return c;
}

char *xml_parse_name(struct xml_context *ctx, struct mempool *pool);
void xml_skip_name(struct xml_context *ctx);
char *xml_parse_nmtoken(struct xml_context *ctx, struct mempool *pool);

char *xml_parse_system_literal(struct xml_context *ctx, struct mempool *pool);
char *xml_parse_pubid_literal(struct xml_context *ctx, struct mempool *pool);

uns xml_parse_char_ref(struct xml_context *ctx);
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
