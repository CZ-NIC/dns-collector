/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "sherlock/xml/xml.h"
#include "sherlock/xml/dtd.h"
#include "sherlock/xml/common.h"
#include "lib/fastbuf.h"
#include "lib/ff-unicode.h"
#include "lib/unicode.h"
#include "lib/chartype.h"
#include "lib/hashfunc.h"

#include <setjmp.h>

/*** Comments ***/

void
xml_push_comment(struct xml_context *ctx)
{
  TRACE(ctx, "push_comment");
  /* Comment ::= '<!--' ((Char - '-') | ('-' (Char - '-')))* '-->'
   * Already parsed: '<!-' */
  xml_parse_char(ctx, '-');
  struct xml_node *n = xml_push_dom(ctx);
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
  if (ctx->h_comment)
    ctx->h_comment(ctx);
}

void
xml_pop_comment(struct xml_context *ctx)
{
  xml_pop_dom(ctx);
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
  struct xml_node *n = xml_push_dom(ctx);
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
  if (ctx->h_pi)
    ctx->h_pi(ctx);
}

void
xml_pop_pi(struct xml_context *ctx)
{
  xml_pop_dom(ctx);
  xml_dec(ctx);
  TRACE(ctx, "pop_pi");
}

void
xml_skip_pi(struct xml_context *ctx)
{
  TRACE(ctx, "skip_pi");
  if (ctx->flags & XML_FLAG_VALIDATING)
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

/*** Character data ***/

static void
xml_chars_spout(struct fastbuf *fb)
{
  if (fb->bptr >= fb->bufend)
    {
      struct xml_context *ctx = SKIP_BACK(struct xml_context, chars, fb);
      struct mempool *pool = ctx->pool;
      if (fb->bufend != fb->buffer)
        {
          uns len = fb->bufend - fb->buffer;
          TRACE(ctx, "grow_chars");
          fb->buffer = mp_expand(pool);
          fb->bufend = fb->buffer + mp_avail(pool);
          fb->bstop = fb->buffer;
          fb->bptr = fb->buffer + len;
	}
      else
        {
	  TRACE(ctx, "push_chars");
          struct xml_node *n = xml_push_dom(ctx);
	  n->type = XML_NODE_CDATA;
	  xml_start_chars(ctx);
	}
    }
}

static void
xml_init_chars(struct xml_context *ctx)
{
  struct fastbuf *fb = &ctx->chars;
  fb->name = "<xml-chars>";
  fb->spout = xml_chars_spout;
  fb->can_overwrite_buffer = 1;
  fb->bptr = fb->bstop = fb->buffer = fb->bufend = NULL;
}

static inline uns
xml_flush_chars(struct xml_context *ctx)
{
  struct fastbuf *fb = &ctx->chars;
  if (fb->bufend == fb->buffer)
    return 0;
  TRACE(ctx, "flush_chars");
  struct xml_node *n = ctx->node;
  n->text = xml_end_chars(ctx, &n->len);
  n->len = fb->bufend - fb->buffer;
  if (ctx->h_chars)
    ctx->h_chars(ctx);
  return 1;
}

static inline void
xml_pop_chars(struct xml_context *ctx)
{
  xml_pop_dom(ctx);
  TRACE(ctx, "pop_chars");
}

static inline void
xml_append_chars(struct xml_context *ctx)
{
  TRACE(ctx, "append_chars");
  struct fastbuf *out = &ctx->chars;
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
xml_push_cdata(struct xml_context *ctx)
{
  TRACE(ctx, "push_cdata");
  /* CDSect :== '<![CDATA[' (Char* - (Char* ']]>' Char*)) ']]>'
   * Already parsed: '<![' */
  xml_parse_seq(ctx, "CDATA[");
  struct xml_node *n = xml_push_dom(ctx);
  n->type = XML_NODE_CDATA;
  char *p = mp_start_noalign(ctx->pool, 7);
  while (1)
    {
      if (xml_get_char(ctx) == ']')
        {
          if (xml_get_char(ctx) == ']')
	    if (xml_get_char(ctx) == '>')
	      break;
	    else
	      *p++ = ']';
	  *p++ = ']';
	}
      p = utf8_32_put(p, xml_last_char(ctx));
      p = mp_spread(ctx->pool, p, 7);
    }
  *p = 0;
  n->len = p - (char *)mp_ptr(ctx->pool);
  n->text = mp_end(ctx->pool, p + 1);
  if (ctx->h_cdata)
    ctx->h_cdata(ctx);
}

static void
xml_pop_cdata(struct xml_context *ctx)
{
  xml_pop_dom(ctx);
  xml_dec(ctx);
  TRACE(ctx, "pop_cdata");
}

static void
xml_append_cdata(struct xml_context *ctx)
{
  TRACE(ctx, "append_cdata");
  xml_parse_seq(ctx, "CDATA[");
  struct fastbuf *out = &ctx->chars;
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
  xml_dec(ctx);
}

static void UNUSED
xml_skip_cdata(struct xml_context *ctx)
{
  TRACE(ctx, "skip_cdata");
  xml_parse_seq(ctx, "CDATA[");
  while (xml_get_char(ctx) != ']' || xml_get_char(ctx) != ']' || xml_get_char(ctx) != '>');
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
  if (!(cat & XML_CHAR_UNRESTRICTED_1_1) && ((ctx->flags & XML_FLAG_VERSION_1_1) || !(cat & XML_CHAR_VALID_1_0)))
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

void
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
      struct xml_dtd_ent *ent = xml_dtd_find_gent(ctx, name);
      if (!ent)
        {
	  xml_error(ctx, "Unknown entity &%s;", name);
	  bputc(out, '&');
	  bputs(out, name);
	  bputc(out, ';');
	}
      else if (ent->flags & XML_DTD_ENT_TRIVIAL)
        {
	  TRACE(ctx, "Trivial entity &%s;", name);
	  bwrite(out, ent->text, ent->len);
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

/*** Attribute values ***/

char *
xml_parse_attr_value(struct xml_context *ctx, struct xml_dtd_attr *attr UNUSED)
{
  TRACE(ctx, "parse_attr_value");
  /* AttValue ::= '"' ([^<&"] | Reference)* '"'	| "'" ([^<&'] | Reference)* "'" */
  /* FIXME:
   * -- copying from ctx->chars to ctx->pool is not necessary, we could directly write to ctx->pool
   * -- berare quotes inside parased entities
   * -- check value constrains / normalize value */
  struct mempool_state state;
  uns quote = xml_parse_quote(ctx);
  mp_save(ctx->stack, &state);
  xml_start_chars(ctx);
  struct fastbuf *out = &ctx->chars;
  while (1)
    {
      uns c = xml_get_char(ctx);
      if (c == '&')
        {
	  xml_inc(ctx);
	  xml_parse_ref(ctx);
	}
      else if (c == quote) // FIXME: beware quotes inside parsed entities
	break;
      else if (c == '<')
	xml_error(ctx, "Attribute value must not contain '<'");
      else if (xml_last_cat(ctx) & XML_CHAR_WHITE)
	bputc(out, ' ');
      else
	bput_utf8_32(out, c);
    }
  mp_restore(ctx->stack, &state);
  uns len;
  return xml_end_chars(ctx, &len);
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
  /* FIXME:
   * -- memory management
   * -- DTD */
  struct xml_node *e = ctx->node;
  char *n = xml_parse_name(ctx, ctx->pool);
  struct xml_attr *a = xml_attrs_lookup(ctx->tab_attrs, e, n);
  xml_parse_eq(ctx);
  char *v = xml_parse_attr_value(ctx, NULL);
  if (a->val)
    xml_error(ctx, "Attribute %s is not unique", n);
  else
    a->val = v;
}

/*** Elements ***/

static void
xml_push_element(struct xml_context *ctx)
{
  TRACE(ctx, "push_element");
  /* EmptyElemTag | STag
   * EmptyElemTag ::= '<' Name (S  Attribute)* S? '/>'
   * STag ::= '<' Name (S  Attribute)* S? '>'
   * Already parsed: '<' */
  struct xml_node *e = xml_push_dom(ctx);
  clist_init(&e->sons);
  e->type = XML_NODE_ELEM;
  e->name = xml_parse_name(ctx, ctx->pool);
  slist_init(&e->attrs);
  if (!e->parent)
    {
      ctx->root = e;
      if (ctx->document_type && strcmp(e->name, ctx->document_type))
	xml_error(ctx, "The root element %s does not match the document type %s", e->name, ctx->document_type);
    }
  while (1)
    {
      uns white = xml_parse_white(ctx, 0);
      uns c = xml_get_char(ctx);
      if (c == '/')
        {
	  xml_parse_char(ctx, '>');
	  ctx->flags |= XML_FLAG_EMPTY_ELEM;
	  break;
	}
      else if (c == '>')
	break;
      else if (!white)
	xml_fatal_expected_white(ctx);
      xml_unget_char(ctx);
      xml_parse_attr(ctx);
    }
  if (ctx->h_element_start)
    ctx->h_element_start(ctx);
}

static void
xml_pop_element(struct xml_context *ctx)
{
  TRACE(ctx, "pop_element");
  if (ctx->h_element_end)
    ctx->h_element_end(ctx);
  struct xml_node *e = ctx->node;
  if (ctx->flags & XML_DOM_FREE)
    {
      if (!e->parent)
	ctx->root = NULL;
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
  xml_pop_dom(ctx);
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
  if (ctx->document_type)
    xml_fatal(ctx, "Multiple document types not allowed");
  xml_parse_seq(ctx, "DOCTYPE");
  xml_parse_white(ctx, 1);
  ctx->document_type = xml_parse_name(ctx, ctx->pool);
  TRACE(ctx, "doctyype=%s", ctx->document_type);
  uns c;
  if (xml_parse_white(ctx, 0) && ((c = xml_peek_char(ctx)) == 'S' || c == 'P'))
    {
      if (c == 'S')
        {
	  xml_parse_seq(ctx, "SYSTEM");
	  xml_parse_white(ctx, 1);
	  ctx->eid.system_id = xml_parse_system_literal(ctx, ctx->pool);
	}
      else
        {
	  xml_parse_seq(ctx, "PUBLIC");
	  xml_parse_white(ctx, 1);
	  ctx->eid.public_id = xml_parse_pubid_literal(ctx, ctx->pool);
	  xml_parse_white(ctx, 1);
	  ctx->eid.system_id = xml_parse_system_literal(ctx, ctx->pool);
	}
      xml_parse_white(ctx, 0);
      ctx->flags |= XML_FLAG_HAS_EXTERNAL_SUBSET;
    }
  if (xml_peek_char(ctx) == '[')
    ctx->flags |= XML_FLAG_HAS_INTERNAL_SUBSET;
  if (ctx->h_doctype_decl)
    ctx->h_doctype_decl(ctx);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////

/* DTD: Internal subset */

static void
xml_parse_internal_subset(struct xml_context *ctx)
{
  // FIXME: comments/pi have no parent
  /* '[' intSubset ']'
   * intSubset :== (markupdecl | DeclSep)
   * Already parsed: ']' */
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
      else if (c == ']')
	break;
      else
	goto invalid_markup;
    }
  xml_dec(ctx);
  xml_dec(ctx);
  return;
invalid_markup:
  xml_fatal(ctx, "Invalid markup in the internal subset");
}


/*----------------------------------------------*/

void
xml_init(struct xml_context *ctx)
{
  bzero(ctx, sizeof(*ctx));
  ctx->pool = mp_new(65536);
  ctx->stack = mp_new(65536);
  ctx->flags = XML_DOM_FREE;
  xml_init_chars(ctx);
  xml_dtd_init(ctx);
  xml_attrs_init(ctx->tab_attrs = xml_hash_new(ctx->pool, sizeof(struct xml_attrs_table)));
}

void
xml_cleanup(struct xml_context *ctx)
{
  xml_attrs_cleanup(ctx->tab_attrs);
  xml_dtd_cleanup(ctx);
  mp_delete(ctx->pool);
  mp_delete(ctx->stack);
}

int
xml_next(struct xml_context *ctx)
{
  /* A nasty state machine */

  TRACE(ctx, "xml_next (state=%u)", ctx->state);
  jmp_buf throw_buf;
  ctx->throw_buf = &throw_buf;
  if (setjmp(throw_buf))
    {
error:
      if (ctx->err_code == XML_ERR_EOF && ctx->h_fatal)
	ctx->h_fatal(ctx);
      ctx->state = XML_STATE_FATAL;
      TRACE(ctx, "raised fatal error");
      return -1;
    }
  uns c;
  switch (ctx->state)
    {
      case XML_STATE_FATAL:
	return -1;

      case XML_STATE_START:
	TRACE(ctx, "entering prolog");
	if (ctx->h_document_start)
	  ctx->h_document_start(ctx);
	/* XMLDecl */
	xml_refill(ctx);
	if (ctx->h_xml_decl)
	  ctx->h_xml_decl(ctx);
	if (ctx->want & XML_WANT_DECL)
	  return ctx->state = XML_STATE_DECL;
      case XML_STATE_DECL:

	/* Misc* (doctypedecl Misc*)? */
        while (1)
	  {
	    xml_parse_white(ctx, 0);
	    xml_parse_char(ctx, '<');
	    if ((c = xml_get_char(ctx)) == '?')
	      /* Processing intruction */
	      if (!(ctx->want & XML_WANT_PI))
	        xml_skip_pi(ctx);
	      else
	        {
		  xml_push_pi(ctx);
		  ctx->state = XML_STATE_PROLOG_PI;
		  return XML_STATE_PI;
      case XML_STATE_PROLOG_PI:
		  xml_pop_pi(ctx);
	        }
	    else if (c != '!')
	      {
		/* Found the root tag */
		xml_unget_char(ctx);
		goto first_tag;
	      }
	    else if (xml_get_char(ctx) == '-')
	      if (!(ctx->want & XML_WANT_COMMENT))
		xml_skip_comment(ctx);
	      else
	        {
		  xml_push_comment(ctx);
		  ctx->state = XML_STATE_PROLOG_COMMENT;
		  return XML_STATE_COMMENT;
      case XML_STATE_PROLOG_COMMENT:
		  xml_pop_comment(ctx);
		}
	    else
	      {
		/* DocTypeDecl */
		xml_unget_char(ctx);
		xml_parse_doctype_decl(ctx);
		if (ctx->want & XML_WANT_DOCUMENT_TYPE)
		  return ctx->state = XML_STATE_DOCUMENT_TYPE;
      case XML_STATE_DOCUMENT_TYPE:
		if (xml_peek_char(ctx) == '[')
		  {
		    xml_skip_char(ctx);
		    xml_inc(ctx);
		    xml_parse_internal_subset(ctx);
		    xml_parse_white(ctx, 0);
		  }
		xml_parse_char(ctx, '>');
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
first_tag: ;

	    xml_inc(ctx);
	    if ((c = xml_get_char(ctx)) == '?')
	      {
		/* PI */
	        if (!(ctx->want & XML_WANT_PI))
	          xml_skip_pi(ctx);
	        else
		  {
		    if (xml_flush_chars(ctx))
		      {
			if (ctx->want & XML_WANT_CHARS)
			  {
			    ctx->state = XML_STATE_CHARS_BEFORE_PI;
			    return XML_STATE_CHARS;
			  }
      case XML_STATE_CHARS_BEFORE_PI:
			xml_pop_chars(ctx);
		      }
		    xml_push_pi(ctx);
		    return ctx->state = XML_STATE_PI;
      case XML_STATE_PI:
		    xml_pop_pi(ctx);
		  }
	      }

	    else if (c == '!')
	      if ((c = xml_get_char(ctx)) == '-')
	        {
		  /* Comment */
		  if (!(ctx->want & XML_WANT_COMMENT))
		    xml_skip_comment(ctx);
		  else
		    {
		      if (xml_flush_chars(ctx))
		        {
			  if (ctx->want & XML_WANT_CHARS)
			    {
			      ctx->state = XML_STATE_CHARS_BEFORE_COMMENT;
			      return XML_STATE_CHARS;
			    }
      case XML_STATE_CHARS_BEFORE_COMMENT:
			  xml_pop_chars(ctx);
			}
		      xml_push_comment(ctx);
		      return ctx->state = XML_STATE_COMMENT;
      case XML_STATE_COMMENT:
		      xml_pop_comment(ctx);
		    }
		}
	      else if (c == '[')
	        {
		  /* CDATA */
		  if (!(ctx->want & XML_WANT_CDATA))
		    xml_append_cdata(ctx);
		  else
		    {
		      if (xml_flush_chars(ctx))
		        {
			  if (ctx->want & XML_WANT_CHARS)
			    {
			      ctx->state = XML_STATE_CHARS_BEFORE_CDATA;
			      return XML_STATE_CHARS;
			    }
      case XML_STATE_CHARS_BEFORE_CDATA:
			  xml_pop_chars(ctx);
			}
		      xml_push_cdata(ctx);
		      return ctx->state = XML_STATE_CDATA;
      case XML_STATE_CDATA:
		      xml_pop_cdata(ctx);
		    }
		}
	      else
		xml_fatal(ctx, "Unexpected character after '<!'");

	    else if (c != '/')
	      {
		/* STag | EmptyElemTag */
		xml_unget_char(ctx);
		if (xml_flush_chars(ctx))
		  {
		    if (ctx->want & XML_WANT_CHARS)
		      {
		        ctx->state = XML_STATE_CHARS_BEFORE_STAG;
		        return XML_STATE_CHARS;
		      }
      case XML_STATE_CHARS_BEFORE_STAG:
		    xml_pop_chars(ctx);
		  }

		xml_push_element(ctx);
		if (ctx->want & XML_WANT_STAG)
		  return ctx->state = XML_STATE_STAG;
      case XML_STATE_STAG:
		if (ctx->flags & XML_FLAG_EMPTY_ELEM)
		  goto pop_element;
	      }

	    else
	      {
		/* ETag */
		if (xml_flush_chars(ctx))
		  {
		    if (ctx->want & XML_WANT_CHARS)
		      {
		        ctx->state = XML_STATE_CHARS_BEFORE_ETAG;
		        return XML_STATE_CHARS;
		      }
      case XML_STATE_CHARS_BEFORE_ETAG:
		    xml_pop_chars(ctx);
		  }

		xml_parse_etag(ctx);
pop_element:
		if (ctx->want & XML_WANT_ETAG)
		  return ctx->state = XML_STATE_ETAG;
      case XML_STATE_ETAG:
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
		  return XML_STATE_EOF;
		}
	      else
		goto error;
	    xml_parse_white(ctx, 0);
	    if (setjmp(throw_buf))
	      goto error;

	    /* Misc */
	    xml_parse_char(ctx, '<');
	    if ((c = xml_get_char(ctx)) == '?')
	      /* Processing instruction */
	      if (!(ctx->want & XML_WANT_PI))
	        xml_skip_pi(ctx);
	      else
	        {
		  xml_push_pi(ctx);
		  return ctx->state = XML_STATE_EPILOG_PI, XML_STATE_PI;
      case XML_STATE_EPILOG_PI:
		  xml_pop_pi(ctx);
	        }
	    else if (c == '!')
	      /* Comment */
	      if (!(ctx->want & XML_WANT_COMMENT))
		xml_skip_comment(ctx);
	      else
	        {
		  xml_push_comment(ctx);
		  return ctx->state = XML_STATE_EPILOG_COMMENT, XML_STATE_COMMENT;
      case XML_STATE_EPILOG_COMMENT:
		  xml_pop_comment(ctx);
		}
	    else
	      xml_fatal(ctx, "Syntax error in the epilog");
	  }

    }
  return -1;
}
