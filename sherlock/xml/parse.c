/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007--2008 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "sherlock/xml/xml.h"
#include "sherlock/xml/dtd.h"
#include "sherlock/xml/internals.h"
#include "lib/fastbuf.h"
#include "lib/ff-unicode.h"
#include "lib/unicode.h"
#include "lib/chartype.h"
#include "lib/hashfunc.h"

#include <setjmp.h>

/*** Basic parsing ***/

void NONRET
xml_fatal_expected(struct xml_context *ctx, uns c)
{
  if (c >= 32 && c < 128)
    xml_fatal(ctx, "Expected '%c'", c);
  else
    xml_fatal(ctx, "Expected U+%04x", c);
}

void NONRET
xml_fatal_expected_white(struct xml_context *ctx)
{
  xml_fatal(ctx, "Expected a white space");
}

void NONRET
xml_fatal_expected_quot(struct xml_context *ctx)
{
  xml_fatal(ctx, "Expected a quotation mark");
}

void
xml_parse_eq(struct xml_context *ctx)
{
  /* Eq ::= S? '=' S? */
  xml_parse_white(ctx, 0);
  xml_parse_char(ctx, '=');
  xml_parse_white(ctx, 0);
}

/*** Names and nmtokens ***/

static char *
xml_parse_string(struct xml_context *ctx, struct mempool *pool, uns first_cat, uns next_cat, char *err)
{
  char *p = mp_start_noalign(pool, 1);
  if (unlikely(!(xml_peek_cat(ctx) & first_cat)))
    xml_fatal(ctx, "%s", err);
  do
    {
      p = mp_spread(pool, p, 5);
      p = utf8_32_put(p, xml_skip_char(ctx));
    }
  while (xml_peek_cat(ctx) & next_cat);
  *p++ = 0;
  return mp_end(pool, p);
}

static void
xml_skip_string(struct xml_context *ctx, uns first_cat, uns next_cat, char *err)
{
  if (unlikely(!(xml_get_cat(ctx) & first_cat)))
    xml_fatal(ctx, "%s", err);
  while (xml_peek_cat(ctx) & next_cat)
    xml_skip_char(ctx);
}

char *
xml_parse_name(struct xml_context *ctx, struct mempool *pool)
{
  /* Name ::= NameStartChar (NameChar)* */
  return xml_parse_string(ctx, pool, ctx->cat_sname, ctx->cat_name, "Expected a name");
}

void
xml_skip_name(struct xml_context *ctx)
{
  xml_skip_string(ctx, ctx->cat_sname, ctx->cat_name, "Expected a name");
}

char *
xml_parse_nmtoken(struct xml_context *ctx, struct mempool *pool)
{
  /* Nmtoken ::= (NameChar)+ */
  return xml_parse_string(ctx, pool, ctx->cat_name, ctx->cat_name, "Expected a nmtoken");
}

/*** Simple literals ***/

char *
xml_parse_system_literal(struct xml_context *ctx, struct mempool *pool)
{
  /* SystemLiteral ::= ('"' [^"]* '"') | ("'" [^']* "'") */
  char *p = mp_start_noalign(pool, 1);
  uns q = xml_parse_quote(ctx), c;
  while ((c = xml_get_char(ctx)) != q)
    {
      p = mp_spread(pool, p, 5);
      p = utf8_32_put(p, c);
    }
  *p++ = 0;
  return mp_end(pool, p);
}

char *
xml_parse_pubid_literal(struct xml_context *ctx, struct mempool *pool)
{
  /* PubidLiteral ::= '"' PubidChar* '"' | "'" (PubidChar - "'")* "'" */
  char *p = mp_start_noalign(pool, 1);
  uns q = xml_parse_quote(ctx), c;
  while ((c = xml_get_char(ctx)) != q)
    {
      if (unlikely(!(xml_last_cat(ctx) & XML_CHAR_PUBID)))
	xml_fatal(ctx, "Expected a pubid character");
      p = mp_spread(pool, p, 2);
      *p++ = c;
    }
  *p++ = 0;
  return mp_end(pool, p);
}

/*** Comments ***/

void
xml_push_comment(struct xml_context *ctx)
{
  TRACE(ctx, "push_comment");
  /* Comment ::= '<!--' ((Char - '-') | ('-' (Char - '-')))* '-->'
   * Already parsed: '<!-' */
  xml_parse_char(ctx, '-');
  struct xml_node *n = xml_push_dom(ctx, NULL);
  n->type = XML_NODE_COMMENT;
  char *p = mp_start_noalign(ctx->pool, 6);
  while (1)
    {
      if (xml_get_char(ctx) == '-')
	if (xml_get_char(ctx) == '-')
	  break;
	else
	  *p++ = '-';
      p = utf8_32_put(p, xml_last_char(ctx));
      p = mp_spread(ctx->pool, p, 6);
    }
  xml_parse_char(ctx, '>');
  *p = 0;
  n->len = p - (char *)mp_ptr(ctx->pool);
  n->text = mp_end(ctx->pool, p + 1);
  if ((ctx->flags & XML_REPORT_COMMENTS) && ctx->h_comment)
    ctx->h_comment(ctx);
}

void
xml_pop_comment(struct xml_context *ctx)
{
  xml_pop_dom(ctx, !(ctx->flags & XML_ALLOC_COMMENTS));
  xml_dec(ctx);
  TRACE(ctx, "pop_comment");
}

void
xml_skip_comment(struct xml_context *ctx)
{
  TRACE(ctx, "skip_comment");
  xml_parse_char(ctx, '-');
  while (xml_get_char(ctx) != '-' || xml_get_char(ctx) != '-');
  xml_parse_char(ctx, '>');
  xml_dec(ctx);
}

/*** Processing instructions ***/

void
xml_push_pi(struct xml_context *ctx)
{
  TRACE(ctx, "push_pi");
  /* Parses a PI to ctx->value and ctx->name:
   *   PI ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
   *   PITarget ::= Name - (('X' | 'x') ('M' | 'm') ('L' | 'l'))
   * Already parsed: '<?' */
  struct xml_node *n = xml_push_dom(ctx, NULL);
  n->type = XML_NODE_PI;
  n->name = xml_parse_name(ctx, ctx->pool);
  if (unlikely(!strcasecmp(n->name, "xml")))
    xml_error(ctx, "Reserved PI target");
  char *p = mp_start_noalign(ctx->pool, 5);
  if (!xml_parse_white(ctx, 0))
    xml_parse_seq(ctx, "?>");
  else
    while (1)
      {
	if (xml_get_char(ctx) == '?')
	  if (xml_peek_char(ctx) == '>')
	    {
	      xml_skip_char(ctx);
	      break;
	    }
	  else
	    *p++ = '?';
	else
	  p = utf8_32_put(p, xml_last_char(ctx));
	p = mp_spread(ctx->pool, p, 5);
      }
  *p = 0;
  n->len = p - (char *)mp_ptr(ctx->pool);
  n->text = mp_end(ctx->pool, p + 1);
  if ((ctx->flags & XML_REPORT_PIS) && ctx->h_pi)
    ctx->h_pi(ctx);
}

void
xml_pop_pi(struct xml_context *ctx)
{
  xml_pop_dom(ctx, !(ctx->flags & XML_ALLOC_PIS));
  xml_dec(ctx);
  TRACE(ctx, "pop_pi");
}

void
xml_skip_pi(struct xml_context *ctx)
{
  TRACE(ctx, "skip_pi");
  if (ctx->flags & XML_VALIDATING)
    {
      struct mempool_state state;
      mp_save(ctx->stack, &state);
      if (unlikely(!strcasecmp(xml_parse_name(ctx, ctx->stack), "xml")))
	xml_error(ctx, "Reserved PI target");
      mp_restore(ctx->stack, &state);
      if (!xml_parse_white(ctx, 0))
        {
	  xml_parse_seq(ctx, "?>");
	  xml_dec(ctx);
	  return;
	}
    }
  while (1)
    if (xml_get_char(ctx) == '?')
      if (xml_peek_char(ctx) == '>')
	break;
  xml_skip_char(ctx);
  xml_dec(ctx);
}

/*** Character references ***/

uns
xml_parse_char_ref(struct xml_context *ctx)
{
  TRACE(ctx, "parse_char_ref");
  /* CharRef ::= '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'
   * Already parsed: '&#' */
  uns v = 0;
  if (xml_get_char(ctx) == 'x')
    {
      if (!(xml_get_cat(ctx) & XML_CHAR_XDIGIT))
        {
	  xml_error(ctx, "Expected a hexadecimal value of character reference");
	  goto recover;
	}
      do
        {
	  v = (v << 4) + Cxvalue(xml_last_char(ctx));
	}
      while (v < 0x110000 && (xml_get_cat(ctx) & XML_CHAR_XDIGIT));
    }
  else
    {
      if (!(xml_last_cat(ctx) & XML_CHAR_DIGIT))
        {
	  xml_error(ctx, "Expected a numeric value of character reference");
	  goto recover;
	}
      do
        {
	  v = v * 10 + xml_last_char(ctx) - '0';
	}
      while (v < 0x110000 && (xml_get_cat(ctx) & XML_CHAR_DIGIT));
    }
  uns cat = xml_char_cat(v);
  if (!(cat & ctx->cat_unrestricted))
    {
      xml_error(ctx, "Character reference out of range");
      goto recover;
    }
  if (xml_last_char(ctx) == ';')
    {
      xml_dec(ctx);
      return v;
    }
  xml_error(ctx, "Expected ';'");
recover:
  while (xml_last_char(ctx) != ';')
    xml_get_char(ctx);
  xml_dec(ctx);
  return UNI_REPLACEMENT;
}

/*** References to general entities ***/

static void
xml_parse_ref(struct xml_context *ctx)
{
  /* Reference ::= EntityRef | CharRef
   * EntityRef ::= '&' Name ';'
   * Already parsed: '&' */
  struct fastbuf *out = &ctx->chars;
  if (xml_peek_char(ctx) == '#')
    {
      xml_skip_char(ctx);
      bput_utf8_32(out, xml_parse_char_ref(ctx));
    }
  else
    {
      TRACE(ctx, "parse_ge_ref");
      struct mempool_state state;
      mp_save(ctx->stack, &state);
      char *name = xml_parse_name(ctx, ctx->stack);
      xml_parse_char(ctx, ';');
      struct xml_dtd_entity *ent = xml_dtd_find_entity(ctx, name);
      if (!ent)
        {
	  xml_error(ctx, "Unknown entity &%s;", name);
	  bputc(out, '&');
	  bputs(out, name);
	  bputc(out, ';');
	}
      else if (ent->flags & XML_DTD_ENTITY_TRIVIAL)
        {
	  TRACE(ctx, "Trivial entity &%s;", name);
	  bputs(out, ent->text);
	}
      else
        {
	  TRACE(ctx, "Pushed entity &%s;", name);
	  mp_restore(ctx->stack, &state);
          xml_dec(ctx);
	  xml_push_entity(ctx, ent);
	  return;
	}
      mp_restore(ctx->stack, &state);
      xml_dec(ctx);
    }
}

/*** Character data ***/

void
xml_spout_chars(struct fastbuf *fb)
{
  if (fb->bptr < fb->bufend)
    return;
  struct xml_context *ctx = SKIP_BACK(struct xml_context, chars, fb);
  struct mempool *pool = ctx->pool;
  if (fb->bufend != fb->buffer)
    {
      TRACE(ctx, "growing chars");
      uns len = fb->bufend - fb->buffer;
      uns reported = fb->bstop - fb->buffer;
      fb->buffer = mp_expand(pool);
      fb->bufend = fb->buffer + mp_avail(pool);
      fb->bptr = fb->buffer + len;
      fb->bstop = fb->buffer + reported;
    }
  else
    {
      TRACE(ctx, "starting chars");
      mp_save(pool, &ctx->chars_state);
      fb->bptr = fb->buffer = fb->bstop = mp_start_noalign(pool, 2);
      fb->bufend = fb->buffer + mp_avail(pool) - 1;
    }
}

static inline uns
xml_end_chars(struct xml_context *ctx, char **out)
{
  struct fastbuf *fb = &ctx->chars;
  uns len = fb->bptr - fb->buffer;
  if (len)
    {
      TRACE(ctx, "ending chars");
      *fb->bptr = 0;
      *out = mp_end(ctx->pool, fb->bptr + 1);
      fb->bufend = fb->bstop = fb->bptr = fb->buffer;
    }
  return len;
}

static inline uns
xml_report_chars(struct xml_context *ctx, char **out)
{
  struct fastbuf *fb = &ctx->chars;
  uns len = fb->bptr - fb->buffer;
  if (len)
    {
      *fb->bptr = 0;
      *out = fb->bstop;
      fb->bstop = fb->bptr;
    }
  return len;
}

static inline uns
xml_flush_chars(struct xml_context *ctx)
{
  char *text, *rtext;
  uns len = xml_end_chars(ctx, &text), rlen;
  if (len)
    {
      if (ctx->flags & XML_NO_CHARS)
        {
          if ((ctx->flags & XML_REPORT_CHARS) && ctx->h_ignorable)
            ctx->h_ignorable(ctx, text, len);
	  mp_restore(ctx->pool, &ctx->chars_state);
	  return 0;
	}
      if ((ctx->flags & XML_REPORT_CHARS) && ctx->h_block && (rlen = xml_report_chars(ctx, &rtext)))
	ctx->h_block(ctx, rtext, rlen);
      if (!(ctx->flags & XML_ALLOC_CHARS) && !(ctx->flags & XML_REPORT_CHARS))
        {
	  mp_restore(ctx->pool, &ctx->chars_state);
	  return 0;
	}
      struct xml_node *n = xml_push_dom(ctx, &ctx->chars_state);
      n->type = XML_NODE_CHARS;
      n->text = text;
      n->len = len;
      if ((ctx->flags & XML_REPORT_CHARS) && ctx->h_chars)
        ctx->h_chars(ctx);
    }
  return len;
}

static inline void
xml_pop_chars(struct xml_context *ctx)
{
  xml_pop_dom(ctx, !(ctx->flags & XML_ALLOC_CHARS));
  TRACE(ctx, "pop_chars");
}

static inline void
xml_append_chars(struct xml_context *ctx)
{
  TRACE(ctx, "append_chars");
  struct fastbuf *out = &ctx->chars;
  if (ctx->flags & XML_NO_CHARS)
    while (xml_get_char(ctx) != '<')
      if (xml_last_cat(ctx) & XML_CHAR_WHITE)
	bput_utf8_32(out, xml_last_char(ctx));
      else
        {
	  xml_error(ctx, "This element must not contain character data");
	  while (xml_get_char(ctx) != '<');
	  break;
	}
  else
    while (xml_get_char(ctx) != '<')
      if (xml_last_char(ctx) == '&')
        {
	  xml_inc(ctx);
          xml_parse_ref(ctx);
        }
      else
        bput_utf8_32(out, xml_last_char(ctx));
  xml_unget_char(ctx);
}

/*** CDATA sections ***/

static void
xml_skip_cdata(struct xml_context *ctx)
{
  TRACE(ctx, "skip_cdata");
  xml_parse_seq(ctx, "CDATA[");
  while (xml_get_char(ctx) != ']' || xml_get_char(ctx) != ']' || xml_get_char(ctx) != '>');
  xml_dec(ctx);
}

static void
xml_append_cdata(struct xml_context *ctx)
{
  /* CDSect :== '<![CDATA[' (Char* - (Char* ']]>' Char*)) ']]>'
   * Already parsed: '<![' */
  TRACE(ctx, "append_cdata");
  if (ctx->flags & XML_NO_CHARS)
    {
      xml_error(ctx, "This element must not contain CDATA");
      xml_skip_cdata(ctx);
      return;
    }
  xml_parse_seq(ctx, "CDATA[");
  struct fastbuf *out = &ctx->chars;
  uns rlen;
  char *rtext;
  if ((ctx->flags & XML_REPORT_CHARS) && ctx->h_block && (rlen = xml_report_chars(ctx, &rtext)))
    ctx->h_block(ctx, rtext, rlen);
  while (1)
    {
      if (xml_get_char(ctx) == ']')
        {
          if (xml_get_char(ctx) == ']')
	    if (xml_get_char(ctx) == '>')
	      break;
	    else
	      bputc(out, ']');
	  bputc(out, ']');
	}
      bput_utf8_32(out, xml_last_char(ctx));
    }
  if ((ctx->flags & XML_REPORT_CHARS) && ctx->h_cdata && (rlen = xml_report_chars(ctx, &rtext)))
    ctx->h_cdata(ctx, rtext, rlen);
  xml_dec(ctx);
}

/*** Attribute values ***/

char *
xml_parse_attr_value(struct xml_context *ctx, struct xml_dtd_attr *attr UNUSED)
{
  TRACE(ctx, "parse_attr_value");
  /* AttValue ::= '"' ([^<&"] | Reference)* '"'	| "'" ([^<&'] | Reference)* "'" */
  /* FIXME: -- check value constrains / normalize leading/trailing WS and repeated WS */
  struct mempool_state state;
  uns quote = xml_parse_quote(ctx);
  mp_save(ctx->stack, &state);
  struct fastbuf *out = &ctx->chars;
  struct xml_source *src = ctx->src;
  while (1)
    {
      uns c = xml_get_char(ctx);
      if (c == '&')
        {
	  xml_inc(ctx);
	  xml_parse_ref(ctx);
	}
      else if (c == quote && src == ctx->src)
	break;
      else if (c == '<')
	xml_error(ctx, "Attribute value must not contain '<'");
      else if (xml_last_cat(ctx) & XML_CHAR_WHITE)
	bputc(out, ' ');
      else
	bput_utf8_32(out, c);
    }
  mp_restore(ctx->stack, &state);
  char *text;
  return xml_end_chars(ctx, &text) ? text : "";
}

uns
xml_normalize_white(struct xml_context *ctx UNUSED, char *text)
{
  char *s = text, *d = text;
  while (*s == 0x20)
    s++;
  while (1)
    {
      while (*s & ~0x20)
	*d++ = *s++;
      if (!*s)
	break;
      while (*++s == 0x20);
      *d++ = 0x20;
    }
  if (d != text && d[-1] == 0x20)
    d--;
  *d = 0;
  return d - text;
}

/*** Attributes ***/

struct xml_attrs_table;

static inline uns
xml_attrs_hash(struct xml_attrs_table *t UNUSED, struct xml_node *e, char *n)
{
  return hash_pointer(e) ^ hash_string(n);
}

static inline int
xml_attrs_eq(struct xml_attrs_table *t UNUSED, struct xml_node *e1, char *n1, struct xml_node *e2, char *n2)
{
  return (e1 == e2) && !strcmp(n1, n2);
}

static inline void
xml_attrs_init_key(struct xml_attrs_table *t UNUSED, struct xml_attr *a, struct xml_node *e, char *name)
{
  a->elem = e;
  a->name = name;
  a->val = NULL;
  a->user = NULL;
  slist_add_tail(&e->attrs, &a->n);
}

#define HASH_PREFIX(x) xml_attrs_##x
#define HASH_NODE struct xml_attr
#define HASH_KEY_COMPLEX(x) x elem, x name
#define HASH_KEY_DECL struct xml_node *elem, char *name
#define HASH_TABLE_DYNAMIC
#define HASH_GIVE_EQ
#define HASH_GIVE_HASHFN
#define HASH_GIVE_INIT_KEY
#define HASH_WANT_CLEANUP
#define HASH_WANT_REMOVE
#define HASH_WANT_LOOKUP
#define HASH_WANT_FIND
#define HASH_GIVE_ALLOC
XML_HASH_GIVE_ALLOC
#include "lib/hashtable.h"

static void
xml_parse_attr(struct xml_context *ctx)
{
  TRACE(ctx, "parse_attr");
  /* Attribute ::= Name Eq AttValue */
  struct xml_node *e = ctx->node;
  char *n = xml_parse_name(ctx, ctx->pool);
  struct xml_attr *a = xml_attrs_lookup(ctx->tab_attrs, e, n);
  xml_parse_eq(ctx);
  char *v = xml_parse_attr_value(ctx, NULL);
  if (a->val)
    {
      xml_error(ctx, "Attribute %s is not unique in element <%s>", n, e->name);
      return;
    }
  a->val = v;
  if (!e->dtd)
    a->dtd = NULL;
  else if (!(a->dtd = xml_dtd_find_attr(ctx, e->dtd, a->name)))
    xml_error(ctx, "Undefined attribute %s in element <%s>", n, e->name);
  else
    xml_validate_attr(ctx, a->dtd, a->val);
}

struct xml_attr *
xml_attr_find(struct xml_context *ctx, struct xml_node *node, char *name)
{
  return xml_attrs_find(ctx->tab_attrs, node, name);
}

char *
xml_attr_value(struct xml_context *ctx, struct xml_node *node, char *name)
{
  struct xml_attr *attr = xml_attrs_find(ctx->tab_attrs, node, name);
  if (attr)
    return attr->val;
  if (!node->dtd)
    return NULL;
  struct xml_dtd_attr *dtd = xml_dtd_find_attr(ctx, node->dtd, name);
  return dtd ? dtd->default_value : NULL;
}

void
xml_attrs_table_init(struct xml_context *ctx)
{
  xml_attrs_init(ctx->tab_attrs = xml_hash_new(ctx->pool, sizeof(struct xml_attrs_table)));
}

void
xml_attrs_table_cleanup(struct xml_context *ctx)
{
  xml_attrs_cleanup(ctx->tab_attrs);
}

/*** Elements ***/

static uns
xml_validate_element(struct xml_dtd_elem_node *root, struct xml_dtd_elem *elem)
{
  if (root->elem)
    return elem == root->elem;
  else
    SLIST_FOR_EACH(struct xml_dtd_elem_node *, son, root->sons)
      if (xml_validate_element(son, elem))
	return 1;
  return 0;
}

static void
xml_push_element(struct xml_context *ctx)
{
  TRACE(ctx, "push_element");
  /* EmptyElemTag | STag
   * EmptyElemTag ::= '<' Name (S  Attribute)* S? '/>'
   * STag ::= '<' Name (S  Attribute)* S? '>'
   * Already parsed: '<' */
  struct xml_node *e = xml_push_dom(ctx, NULL);
  clist_init(&e->sons);
  e->type = XML_NODE_ELEM;
  e->name = xml_parse_name(ctx, ctx->pool);
  slist_init(&e->attrs);
  if (!e->parent)
    {
      ctx->dom = e;
      if (ctx->doctype && strcmp(e->name, ctx->doctype))
	xml_error(ctx, "The root element <%s> does not match the document type <%s>", e->name, ctx->doctype);
    }
  if (!ctx->dtd)
    e->dtd = NULL;
  else if (!(e->dtd = xml_dtd_find_elem(ctx, e->name)))
    xml_error(ctx, "Undefined element <%s>", e->name);
  else
    {
      struct xml_dtd_elem *dtd = e->dtd, *parent_dtd = e->parent ? e->parent->dtd : NULL;
      if (dtd->type == XML_DTD_ELEM_MIXED)
        ctx->flags &= ~XML_NO_CHARS;
      else
	ctx->flags |= XML_NO_CHARS;
      if (parent_dtd)
        if (parent_dtd->type == XML_DTD_ELEM_EMPTY)
	  xml_error(ctx, "Empty element must not contain children");
        else if (parent_dtd->type != XML_DTD_ELEM_ANY)
	  {
	    // FIXME: validate regular expressions
	    if (!xml_validate_element(parent_dtd->node, dtd))
	      xml_error(ctx, "Unexpected element <%s>", e->name);
	  }
    }
  while (1)
    {
      uns white = xml_parse_white(ctx, 0);
      uns c = xml_get_char(ctx);
      if (c == '/')
        {
	  xml_parse_char(ctx, '>');
	  ctx->flags |= XML_EMPTY_ELEM_TAG;
	  break;
	}
      else if (c == '>')
	break;
      else if (!white)
	xml_fatal_expected_white(ctx);
      xml_unget_char(ctx);
      xml_parse_attr(ctx);
    }
  if (e->dtd)
    SLIST_FOR_EACH(struct xml_dtd_attr *, a, e->dtd->attrs)
      if (a->default_mode == XML_ATTR_REQUIRED)
        {
	  if (!xml_attrs_find(ctx->tab_attrs, e, a->name))
	    xml_error(ctx, "Missing required attribute %s in element <%s>", a->name, e->name);
	}
      else if (a->default_mode != XML_ATTR_IMPLIED && ctx->flags & XML_ALLOC_DEFAULT_ATTRS)
        {
	  struct xml_attr *attr = xml_attrs_lookup(ctx->tab_attrs, e, a->name);
	  if (!attr->val)
	    attr->val = a->default_value;
	}
  if ((ctx->flags & XML_REPORT_TAGS) && ctx->h_stag)
    ctx->h_stag(ctx);
}

static void
xml_pop_element(struct xml_context *ctx)
{
  TRACE(ctx, "pop_element");
  if ((ctx->flags & XML_REPORT_TAGS) && ctx->h_etag)
    ctx->h_etag(ctx);
  struct xml_node *e = ctx->node;
  uns free = !(ctx->flags & XML_ALLOC_TAGS);
  if (free)
    {
      if (!e->parent)
	ctx->dom = NULL;
      /* Restore hash table of attributes */
      SLIST_FOR_EACH(struct xml_attr *, a, e->attrs)
	xml_attrs_remove(ctx->tab_attrs, a);
      struct xml_node *n;
      while (n = clist_head(&e->sons))
        {
	  if (n->type == XML_NODE_ELEM)
	    {
	      SLIST_FOR_EACH(struct xml_attr *, a, n->attrs)
		xml_attrs_remove(ctx->tab_attrs, a);
	      clist_insert_list_after(&n->sons, &n->n);
	    }
	  clist_remove(&n->n);
	}
    }
  xml_pop_dom(ctx, free);
  xml_dec(ctx);
}

static void
xml_parse_etag(struct xml_context *ctx)
{
 /* ETag ::= '</' Name S? '>'
  * Already parsed: '<' */
  struct xml_node *e = ctx->node;
  ASSERT(e);
  char *n = e->name;
  while (*n)
    {
      uns c;
      n = utf8_32_get(n, &c);
      if (xml_get_char(ctx) != c)
	goto recover;
    }
  xml_parse_white(ctx, 0);
  if (xml_get_char(ctx) != '>')
    {
recover:
      xml_error(ctx, "Invalid ETag, expected </%s>", e->name);
      while (xml_get_char(ctx) != '>');
    }
  xml_dec(ctx);
}

/*** Document type declaration ***/

static void
xml_parse_doctype_decl(struct xml_context *ctx)
{
  TRACE(ctx, "parse_doctype_decl");
  /* doctypedecl ::= '<!DOCTYPE' S  Name (S  ExternalID)? S? ('[' intSubset ']' S?)? '>'
   * Already parsed: '<!'
   * Terminated before '[' or '>' */
  if (ctx->doctype)
    xml_fatal(ctx, "Multiple document types not allowed");
  xml_parse_seq(ctx, "DOCTYPE");
  xml_parse_white(ctx, 1);
  ctx->doctype = xml_parse_name(ctx, ctx->pool);
  TRACE(ctx, "doctype=%s", ctx->doctype);
  uns c;
  if (xml_parse_white(ctx, 0) && ((c = xml_peek_char(ctx)) == 'S' || c == 'P'))
    {
      if (c == 'S')
        {
	  xml_parse_seq(ctx, "SYSTEM");
	  xml_parse_white(ctx, 1);
	  ctx->system_id = xml_parse_system_literal(ctx, ctx->pool);
	}
      else
        {
	  xml_parse_seq(ctx, "PUBLIC");
	  xml_parse_white(ctx, 1);
	  ctx->public_id = xml_parse_pubid_literal(ctx, ctx->pool);
	  xml_parse_white(ctx, 1);
	  ctx->system_id = xml_parse_system_literal(ctx, ctx->pool);
	}
      xml_parse_white(ctx, 0);
      ctx->flags |= XML_HAS_EXTERNAL_SUBSET;
    }
  if (xml_peek_char(ctx) == '[')
    {
      ctx->flags |= XML_HAS_INTERNAL_SUBSET;
      xml_skip_char(ctx);
      xml_inc(ctx);
    }
  if (ctx->h_doctype_decl)
    ctx->h_doctype_decl(ctx);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////

/* DTD: Internal subset */

static void
xml_parse_subset(struct xml_context *ctx, uns external)
{
  // FIXME:
  // -- comments/pi have no parent
  // -- conditional sections in external subset
  // -- check corectness of parameter entities

  /* '[' intSubset ']'
   * intSubset :== (markupdecl | DeclSep)
   * Already parsed: '['
   *
   * extSubsetDecl ::= ( markupdecl | conditionalSect | DeclSep)*
   */
  while (1)
    {
      xml_parse_white(ctx, 0);
      uns c = xml_get_char(ctx);
      xml_inc(ctx);
      if (c == '<')
	if ((c = xml_get_char(ctx)) == '!')
	  switch (c = xml_get_char(ctx))
	    {
	      case '-':
		xml_push_comment(ctx);
		xml_pop_comment(ctx);
		break;
	      case 'N':
		xml_parse_seq(ctx, "OTATION");
		xml_parse_notation_decl(ctx);
		break;
	      case 'E':
		if ((c = xml_get_char(ctx)) == 'N')
		  {
		    xml_parse_seq(ctx, "TITY");
		    xml_parse_entity_decl(ctx);
		  }
		else if (c == 'L')
		  {
		    xml_parse_seq(ctx, "EMENT");
		    xml_parse_element_decl(ctx);
		  }
		else
		  goto invalid_markup;
		break;
	      case 'A':
		xml_parse_seq(ctx, "TTLIST");
		xml_parse_attr_list_decl(ctx);
		break;
	      default:
		goto invalid_markup;
	    }
        else if (c == '?')
	  {
	    xml_push_pi(ctx);
	    xml_pop_pi(ctx);
	  }
        else
	  goto invalid_markup;
      else if (c == '%')
	xml_parse_pe_ref(ctx);
      else if (c == ']' && !external)
        {
	  break;
	}
      else if (c == '>' && external)
        {
	  break;
	}
      else
	goto invalid_markup;
    }
  xml_dec(ctx);
  return;
invalid_markup: ;
  xml_fatal(ctx, "Invalid markup in the %s subset", external ? "external" : "internal");
}

/*** The State Machine ***/

uns
xml_next(struct xml_context *ctx)
{
  /* A nasty state machine */

#define PULL(x) do { if (ctx->pull & XML_PULL_##x) return ctx->state = XML_STATE_##x; case XML_STATE_##x: ; } while (0)
#define PULL_STATE(x, s) do { if (ctx->pull & XML_PULL_##x) return ctx->state = XML_STATE_##s, XML_STATE_##x; case XML_STATE_##s: ; } while (0)

  TRACE(ctx, "xml_next (state=%u)", ctx->state);
  jmp_buf throw_buf;
  ctx->throw_buf = &throw_buf;
  if (setjmp(throw_buf))
    {
error:
      if (ctx->err_code == XML_ERR_EOF && ctx->h_fatal)
	ctx->h_fatal(ctx);
      TRACE(ctx, "raised fatal error");
      return ctx->state = XML_STATE_EOF;
    }
  uns c;
  switch (ctx->state)
    {
      case XML_STATE_START:
	TRACE(ctx, "entering prolog");
	ctx->flags |= XML_SRC_DOCUMENT | XML_SRC_EXPECTED_DECL;
	if (ctx->h_document_start)
	  ctx->h_document_start(ctx);
	/* XMLDecl */
	xml_refill(ctx);
	if (ctx->h_xml_decl)
	  ctx->h_xml_decl(ctx);
	PULL(XML_DECL);

	/* Misc* (doctypedecl Misc*)? */
        while (1)
	  {
	    xml_parse_white(ctx, 0);
	    xml_parse_char(ctx, '<');
	    xml_inc(ctx);
	    if ((c = xml_get_char(ctx)) == '?')
	      /* Processing intruction */
	      if (!(ctx->flags & XML_REPORT_PIS))
	        xml_skip_pi(ctx);
	      else
	        {
		  xml_push_pi(ctx);
		  PULL_STATE(PI, PROLOG_PI);
		  xml_pop_pi(ctx);
	        }
	    else if (c != '!')
	      {
		/* Found the root tag */
		xml_unget_char(ctx);
		goto first_tag;
	      }
	    else if (xml_get_char(ctx) == '-')
	      if (!(ctx->flags & XML_REPORT_COMMENTS))
		xml_skip_comment(ctx);
	      else
	        {
		  xml_push_comment(ctx);
		  PULL_STATE(COMMENT, PROLOG_COMMENT);
		  xml_pop_comment(ctx);
		}
	    else
	      {
		/* DocTypeDecl */
		xml_unget_char(ctx);
		xml_parse_doctype_decl(ctx);
		PULL(DOCTYPE_DECL);
		if (ctx->flags & XML_HAS_DTD)
		  if (ctx->flags & XML_PARSE_DTD)
		    {
		      xml_dtd_init(ctx);
		      if (ctx->h_dtd_start)
		        ctx->h_dtd_start(ctx);
		      if (ctx->flags & XML_HAS_INTERNAL_SUBSET)
		        {
		          xml_parse_subset(ctx, 0);
			  xml_dec(ctx);
			}
		      if (ctx->flags & XML_HAS_EXTERNAL_SUBSET)
		        {
		          struct xml_dtd_entity ent = {
			    .system_id = ctx->system_id,
			    .public_id = ctx->public_id,
			  };
			  xml_parse_white(ctx, 0);
			  xml_parse_char(ctx, '>');
			  xml_unget_char(ctx);
			  ASSERT(ctx->h_resolve_entity);
			  ctx->h_resolve_entity(ctx, &ent);
			  ctx->flags |= XML_SRC_EXPECTED_DECL;
			  xml_parse_subset(ctx, 1);
			  xml_unget_char(ctx);;
			}
		      if (ctx->h_dtd_end)
		        ctx->h_dtd_end(ctx);
		    }
		  else if (ctx->flags & XML_HAS_INTERNAL_SUBSET)
		    xml_skip_internal_subset(ctx);
		xml_parse_white(ctx, 0);
		xml_parse_char(ctx, '>');
		xml_dec(ctx);
	      }
	  }

      case XML_STATE_CHARS:

	while (1)
	  {
	    if (xml_peek_char(ctx) != '<')
	      {
		/* CharData */
	        xml_append_chars(ctx);
		continue;
	      }
	    else
	      xml_skip_char(ctx);
	    xml_inc(ctx);
first_tag:

	    if ((c = xml_get_char(ctx)) == '?')
	      {
		/* PI */
	        if (!(ctx->flags & (XML_REPORT_PIS | XML_ALLOC_PIS)))
	          xml_skip_pi(ctx);
	        else
		  {
		    if (xml_flush_chars(ctx))
		      {
			PULL_STATE(CHARS, CHARS_BEFORE_PI);
			xml_pop_chars(ctx);
		      }
		    xml_push_pi(ctx);
		    PULL(PI);
		    xml_pop_pi(ctx);
		  }
	      }

	    else if (c == '!')
	      if ((c = xml_get_char(ctx)) == '-')
	        {
		  /* Comment */
		  if (!(ctx->flags & (XML_REPORT_COMMENTS | XML_ALLOC_COMMENTS)))
		    xml_skip_comment(ctx);
		  else
		    {
		      if (xml_flush_chars(ctx))
		        {
			  PULL_STATE(CHARS, CHARS_BEFORE_COMMENT);
			  xml_pop_chars(ctx);
			}
		      xml_push_comment(ctx);
		      PULL(COMMENT);
		      xml_pop_comment(ctx);
		    }
		}
	      else if (c == '[')
	        {
		  /* CDATA */
		  xml_append_cdata(ctx);
		}
	      else
		xml_fatal(ctx, "Unexpected character after '<!'");

	    else if (c != '/')
	      {
		/* STag | EmptyElemTag */
		xml_unget_char(ctx);
		if (xml_flush_chars(ctx))
		  {
		    PULL_STATE(CHARS, CHARS_BEFORE_STAG);
		    xml_pop_chars(ctx);
		  }

		xml_push_element(ctx);
		PULL(STAG);
		if (ctx->flags & XML_EMPTY_ELEM_TAG)
		  goto pop_element;
	      }

	    else
	      {
		/* ETag */
		if (xml_flush_chars(ctx))
		  {
		    PULL_STATE(CHARS, CHARS_BEFORE_ETAG);
		    xml_pop_chars(ctx);
		  }

		xml_parse_etag(ctx);
pop_element:
		PULL(ETAG);
		xml_pop_element(ctx);
		if (!ctx->node)
		  goto epilog;
	      }
	  }

epilog:
	/* Misc* */
        TRACE(ctx, "entering epilog");
	while (1)
	  {
	    /* Epilog whitespace is the only place, where a valid document can reach EOF */
	    if (setjmp(throw_buf))
	      if (ctx->err_code == XML_ERR_EOF)
	        {
		  TRACE(ctx, "reached EOF");
		  ctx->state = XML_STATE_EOF;
		  if (ctx->h_document_end)
		    ctx->h_document_end(ctx);
      case XML_STATE_EOF:
		  ctx->err_code = 0;
		  ctx->err_msg = NULL;
		  return XML_STATE_EOF;
		}
	      else
		goto error;
	    xml_parse_white(ctx, 0);
	    if (setjmp(throw_buf))
	      goto error;

	    /* Misc */
	    xml_parse_char(ctx, '<');
	    xml_inc(ctx);
	    if ((c = xml_get_char(ctx)) == '?')
	      /* Processing instruction */
	      if (!(ctx->flags & XML_REPORT_PIS))
	        xml_skip_pi(ctx);
	      else
	        {
		  xml_push_pi(ctx);
		  PULL_STATE(PI, EPILOG_PI);
		  xml_pop_pi(ctx);
	        }
	    else if (c == '!')
	      {
		xml_parse_char(ctx, '-');
	        /* Comment */
	        if (!(ctx->flags & XML_REPORT_COMMENTS))
		  xml_skip_comment(ctx);
	        else
	          {
		    xml_push_comment(ctx);
		    PULL_STATE(COMMENT, EPILOG_COMMENT);
		    xml_pop_comment(ctx);
		  }
	      }
	    else
	      xml_fatal(ctx, "Syntax error in the epilog");
	  }

    }
  ASSERT(0);
}

uns
xml_next_state(struct xml_context *ctx, uns pull)
{
  uns saved = ctx->pull;
  ctx->pull = pull;
  uns res = xml_next(ctx);
  ctx->pull = saved;
  return res;
}

uns
xml_skip_element(struct xml_context *ctx)
{
  ASSERT(ctx->state == XML_STATE_STAG);
  struct xml_node *node = ctx->node;
  uns saved = ctx->pull, res;
  ctx->pull = XML_PULL_ETAG;
  while ((res = xml_next(ctx)) && ctx->node != node);
  ctx->pull = saved;
  return res;
}

uns
xml_parse(struct xml_context *ctx)
{
  /* This cycle should run only once unless the user overrides the value of ctx->pull in a SAX handler */
  do
    {
      ctx->pull = 0;
    }
  while (xml_next(ctx));
  return ctx->err_code;
}

char *
xml_merge_chars(struct xml_context *ctx UNUSED, struct xml_node *node, struct mempool *pool)
{
  ASSERT(node->type == XML_NODE_ELEM);
  char *p = mp_start_noalign(pool, 1);
  XML_NODE_FOR_EACH(son, node)
    if (son->type == XML_NODE_CHARS)
      {
	p = mp_spread(pool, p, son->len + 1);
	memcpy(p, son->text, son->len);
      }
  *p++ = 0;
  return mp_end(pool, p);
}

static char *
xml_append_dom_chars(char *p, struct mempool *pool, struct xml_node *node)
{
  XML_NODE_FOR_EACH(son, node)
    if (son->type == XML_NODE_CHARS)
      {
	p = mp_spread(pool, p, son->len + 1);
	memcpy(p, son->text, son->len);
      }
    else if (son->type == XML_NODE_ELEM)
      p = xml_append_dom_chars(p, pool, son);
  return p;
}

char *
xml_merge_dom_chars(struct xml_context *ctx UNUSED, struct xml_node *node, struct mempool *pool)
{
  ASSERT(node->type == XML_NODE_ELEM);
  char *p = mp_start_noalign(pool, 1);
  p = xml_append_dom_chars(p, pool, node);
  *p++ = 0;
  return mp_end(pool, p);
}
