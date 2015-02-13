/*
 *	UCW Library -- A simple XML parser
 *
 *	(c) 2007--2008 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw-xml/xml.h>
#include <ucw-xml/dtd.h>
#include <ucw-xml/internals.h>
#include <ucw/unicode.h>
#include <ucw/ff-unicode.h>
#include <charset/charconv.h>
#include <charset/fb-charconv.h>

/*** Character categorization ***/

#include "obj/ucw-xml/unicat.c"

static void
xml_init_cats(struct xml_context *ctx)
{
  if (!(ctx->flags & XML_VERSION_1_1))
    {
      ctx->cat_chars = XML_CHAR_VALID_1_0;
      ctx->cat_unrestricted = XML_CHAR_VALID_1_0;
      ctx->cat_new_line = XML_CHAR_NEW_LINE_1_0;
      ctx->cat_name = XML_CHAR_NAME_1_0;
      ctx->cat_sname = XML_CHAR_SNAME_1_0;
    }
  else
    {
      ctx->cat_chars = XML_CHAR_VALID_1_1;
      ctx->cat_unrestricted = XML_CHAR_UNRESTRICTED_1_1;
      ctx->cat_new_line = XML_CHAR_NEW_LINE_1_1;
      ctx->cat_name = XML_CHAR_NAME_1_1;
      ctx->cat_sname = XML_CHAR_SNAME_1_1;
    }
}

/*** Reading of document/external entities ***/

static void NONRET
xml_eof(struct xml_context *ctx)
{
  ctx->err_msg = "Unexpected EOF";
  ctx->err_code = XML_ERR_EOF;
  xml_throw(ctx);
}

void NONRET
xml_fatal_nested(struct xml_context *ctx)
{
  xml_fatal(ctx, "Entity is not nested correctly");
}

static inline void
xml_add_char(u32 **bstop, uint c)
{
  *(*bstop)++ = c;
  *(*bstop)++ = xml_char_cat(c);
}

struct xml_source *
xml_push_source(struct xml_context *ctx)
{
  xml_push(ctx);
  struct xml_source *src = ctx->src;
  if (src)
    {
      src->bptr = ctx->bptr;
      src->bstop = ctx->bstop;
    }
  src = mp_alloc_zero(ctx->stack, sizeof(*src));
  src->next = ctx->src;
  src->saved_depth = ctx->depth;
  ctx->src = src;
  ctx->flags &= ~(XML_SRC_EOF | XML_SRC_EXPECTED_DECL | XML_SRC_DOCUMENT);
  ctx->bstop = ctx->bptr = src->buf;
  ctx->depth = 0;
  return src;
}

struct xml_source *
xml_push_fastbuf(struct xml_context *ctx, struct fastbuf *fb)
{
  struct xml_source *src = xml_push_source(ctx);
  src->fb = fb;
  return src;
}

static void
xml_close_source(struct xml_source *src)
{
  bclose(src->fb);
  if (src->wrapped_fb)
    bclose(src->wrapped_fb);
}

static void
xml_pop_source(struct xml_context *ctx)
{
  TRACE(ctx, "pop_source");
  if (unlikely(ctx->depth != 0))
    xml_fatal(ctx, "Unexpected end of entity");
  struct xml_source *src = ctx->src;
  if (!src)
    xml_fatal(ctx, "Undefined source");
  xml_close_source(src);
  ctx->depth = src->saved_depth;
  ctx->src = src = src->next;
  if (src)
    {
      ctx->bptr = src->bptr;
      ctx->bstop = src->bstop;
    }
  xml_pop(ctx);
  if (unlikely(!src))
    xml_eof(ctx);
}

void
xml_sources_cleanup(struct xml_context *ctx)
{
  struct xml_source *s;
  while (s = ctx->src)
    {
      ctx->src = s->next;
      xml_close_source(s);
    }
}

static void xml_refill_utf8(struct xml_context *ctx);

void
xml_def_resolve_entity(struct xml_context *ctx, struct xml_dtd_entity *ent UNUSED)
{
  xml_error(ctx, "References to external entities are not supported");
}

void
xml_push_entity(struct xml_context *ctx, struct xml_dtd_entity *ent)
{
  TRACE(ctx, "xml_push_entity");
  struct xml_source *src;
  if (ent->flags & XML_DTD_ENTITY_EXTERNAL)
    {
      ASSERT(ctx->h_resolve_entity);
      ctx->h_resolve_entity(ctx, ent);
      ctx->flags |= XML_SRC_EXPECTED_DECL;
      src = ctx->src;
    }
  else
    {
      src = xml_push_source(ctx);
      fbbuf_init_read(src->fb = &src->wrap_fb, ent->text, strlen(ent->text), 0);
    }
  src->refill = xml_refill_utf8;
  src->refill_cat1 = ctx->cat_unrestricted & ~ctx->cat_new_line;
  src->refill_cat2 = ctx->cat_new_line;
}

static uint
xml_error_restricted(struct xml_context *ctx, uint c)
{
  if (c == ~1U)
    xml_error(ctx, "Corrupted encoding");
  else
    xml_error(ctx, "Restricted char U+%04X", c);
  return UNI_REPLACEMENT;
}

static void xml_parse_decl(struct xml_context *ctx);

#define REFILL(ctx, func, params...)							\
  struct xml_source *src = ctx->src;							\
  struct fastbuf *fb = src->fb;								\
  if (ctx->bptr == ctx->bstop)								\
    ctx->bptr = ctx->bstop = src->buf;							\
  uint c, t1 = src->refill_cat1, t2 = src->refill_cat2, row = src->row;			\
  u32 *bend = src->buf + ARRAY_SIZE(src->buf), *bstop = ctx->bstop,			\
      *last_0xd = src->pending_0xd ? bstop : NULL;					\
  do											\
    {											\
      c = func(fb, ##params);								\
      uint t = xml_char_cat(c);								\
      if (t & t1)									\
        /* Typical branch */								\
	*bstop++ = c, *bstop++ = t;							\
      else if (t & t2)									\
        {										\
	  /* New line */								\
	  /* XML 1.0: 0xA | 0xD | 0xD 0xA */						\
	  /* XML 1.1: 0xA | 0xD | 0xD 0xA | 0x85 | 0xD 0x85 | 0x2028 */			\
	  if (c == 0xd)									\
	    last_0xd = bstop + 2;							\
	  else if (c != 0x2028 && last_0xd == bstop)					\
	    {										\
	      last_0xd = NULL;								\
	      continue;									\
	    }										\
	  xml_add_char(&bstop, 0xa), row++;						\
	}										\
      else if (c == '>')								\
        {										\
	  /* Used only in XML/TextDecl to switch the encoding */			\
	  *bstop++ = c, *bstop++ = t;							\
	  break;									\
	}										\
      else if (~c)									\
        /* Restricted character */							\
        xml_add_char(&bstop, xml_error_restricted(ctx, c));				\
      else										\
        {										\
	  /* EOF */									\
          ctx->flags |= XML_SRC_EOF;							\
          break;									\
	}										\
    }											\
  while (bstop < bend);									\
  src->pending_0xd = (last_0xd == bstop);						\
  ctx->bstop = bstop;									\
  src->row = row;

static void
xml_refill_utf8(struct xml_context *ctx)
{
  REFILL(ctx, bget_utf8_repl, ~1U);
}

static void
xml_refill_utf16_le(struct xml_context *ctx)
{
  REFILL(ctx, bget_utf16_le_repl, ~1U);
}

static void
xml_refill_utf16_be(struct xml_context *ctx)
{
  REFILL(ctx, bget_utf16_be_repl, ~1U);
}

#undef REFILL

void
xml_refill(struct xml_context *ctx)
{
  do
    {
      if (ctx->flags & XML_SRC_EOF)
	xml_pop_source(ctx);
      else if (ctx->flags & XML_SRC_EXPECTED_DECL)
	xml_parse_decl(ctx);
      else
        {
	  ctx->src->refill(ctx);
	  TRACE(ctx, "refilled %u characters", (uint)((ctx->bstop - ctx->bptr) / 2));
	}
    }
  while (ctx->bptr == ctx->bstop);
}

static uint
xml_source_row(struct xml_context *ctx, struct xml_source *src)
{
  uint row = src->row;
  for (u32 *p = ctx->bstop; p != ctx->bptr; p -= 2)
    if (p[-1] & src->refill_cat2)
      row--;
  return row + 1;
}

uint
xml_row(struct xml_context *ctx)
{
  return ctx->src ? xml_source_row(ctx, ctx->src) : 0;
}

/* Document/external entity header */

static char *
xml_parse_encoding_name(struct xml_context *ctx)
{
  /* EncName ::= '"' [A-Za-z] ([A-Za-z0-9._] | '-')* '"' | "'" [A-Za-z] ([A-Za-z0-9._] | '-')* "'" */
  char *p = mp_start_noalign(ctx->pool, 1);
  uint q = xml_parse_quote(ctx);
  if (unlikely(!(xml_get_cat(ctx) & XML_CHAR_ENC_SNAME)))
    xml_fatal(ctx, "Invalid character in the encoding name");
  while (1)
    {
      p = mp_spread(ctx->pool, p, 2);
      *p++ = xml_last_char(ctx);
      if (xml_get_char(ctx) == q)
	break;
      if (unlikely(!(xml_last_cat(ctx) & XML_CHAR_ENC_NAME)))
	xml_fatal(ctx, "Invalid character in the encoding name");
    }
  *p++ = 0;
  return mp_end(ctx->pool, p);
}

static void
xml_init_charconv(struct xml_context *ctx, int cs)
{
  // XXX: with a direct access to libucw-charset tables could be faster
  struct xml_source *src = ctx->src;
  TRACE(ctx, "wrapping charset %s", charset_name(cs));
  src->wrapped_fb = src->fb;
  src->fb = fb_wrap_charconv_in(src->fb, cs, CONV_CHARSET_UTF8);
}

static void
xml_parse_decl(struct xml_context *ctx)
{
  TRACE(ctx, "xml_parse_decl");
  struct xml_source *src = ctx->src;
  ctx->flags &= ~XML_SRC_EXPECTED_DECL;
  uint doc = ctx->flags & XML_SRC_DOCUMENT;

  /* Setup valid Unicode ranges and force the reader to abort refill() after each '>', where we can switch encoding or XML version */
  if (doc)
    xml_init_cats(ctx);
  src->refill_cat1 = ctx->cat_unrestricted & ~ctx->cat_new_line & ~XML_CHAR_GT;
  src->refill_cat2 = ctx->cat_new_line;

  /* Initialize the supplied charset (if any) or try to guess it */
  char *expected_encoding = src->expected_encoding;
  src->refill = xml_refill_utf8;
  int bom = bpeekc(src->fb);
  if (bom < 0)
    ctx->flags |= XML_SRC_EOF;
  if (!src->fb_encoding)
    {
      if (bom == 0xfe)
	src->refill = xml_refill_utf16_be;
      else if (bom == 0xff)
	src->refill = xml_refill_utf16_le;
    }
  else
    {
      int cs = find_charset_by_name(src->fb_encoding);
      if (cs == CONV_CHARSET_UTF8)
        {}
      else if (cs >= 0)
        {
	  xml_init_charconv(ctx, cs);
	  bom = 0;
	}
      else if (strcasecmp(src->fb_encoding, "UTF-16"))
        {
	  src->refill = xml_refill_utf16_be;
	  if (bom == 0xff)
	    src->refill = xml_refill_utf16_le;
	}
      else if (strcasecmp(src->fb_encoding, "UTF-16BE"))
	src->refill = xml_refill_utf16_be;
      else if (strcasecmp(src->fb_encoding, "UTF-16LE"))
	src->refill = xml_refill_utf16_le;
      else
        {
	  xml_error(ctx, "Unknown encoding '%s'", src->fb_encoding);
	  expected_encoding = NULL;
	}
    }
  uint utf16 = src->refill == xml_refill_utf16_le || src->refill == xml_refill_utf16_be;
  if (utf16)
    src->fb_encoding = (src->refill == xml_refill_utf16_be) ? "UTF-16BE" : "UTF-16LE";
  if (!expected_encoding)
    expected_encoding = src->fb_encoding;
  if (bom > 0 && xml_peek_char(ctx) == 0xfeff)
    xml_skip_char(ctx);
  else if (utf16)
    xml_error(ctx, "Missing or corrupted BOM");
  TRACE(ctx, "Initial encoding=%s", src->fb_encoding ? : "?");

  /* Look ahead for presence of XMLDecl or optional TextDecl */
  if (!(ctx->flags & XML_SRC_EOF) && ctx->bstop != src->buf + ARRAY_SIZE(src->buf))
    xml_refill(ctx);
  u32 *bptr = ctx->bptr;
  uint have_decl = (12 <= ctx->bstop - ctx->bptr && (bptr[11] & XML_CHAR_WHITE) &&
    bptr[0] == '<' && bptr[2] == '?' && (bptr[4] & 0xdf) == 'X' && (bptr[6] & 0xdf) == 'M' && (bptr[8] & 0xdf) == 'L');
  if (!have_decl)
    {
      if (doc)
        xml_fatal(ctx, "Missing or corrupted XML header");
      else if (expected_encoding && strcasecmp(src->expected_encoding, "UTF-8") && !utf16)
	xml_error(ctx, "Missing or corrupted entity header");
      goto exit;
    }
  ctx->bptr = bptr + 12;
  xml_parse_white(ctx, 0);

  /* Parse version string (mandatory in XMLDecl, optional in TextDecl) */
  if (xml_peek_char(ctx) == 'v')
    {
      xml_parse_seq(ctx, "version");
      xml_parse_eq(ctx);
      char *version = xml_parse_pubid_literal(ctx, ctx->pool);
      TRACE(ctx, "version=%s", version);
      uint v = 0;
      if (!strcmp(version, "1.1"))
	v = XML_VERSION_1_1;
      else if (strcmp(version, "1.0"))
        {
	  xml_error(ctx, "Unknown XML version string '%s'", version);
	  version = "1.0";
	}
      if (doc)
        {
	  ctx->version_str = version;
	  ctx->flags |= v;
	}
      else if (v > (ctx->flags & XML_VERSION_1_1))
        xml_error(ctx, "XML 1.1 external entity included from XML 1.0 document");
      if (!xml_parse_white(ctx, !doc))
        goto end;
    }
  else if (doc)
    {
      xml_error(ctx, "Expected XML version");
      ctx->version_str = "1.0";
    }

  /* Parse encoding string (optional in XMLDecl, mandatory in TextDecl) */
  if (xml_peek_char(ctx) == 'e')
    {
      xml_parse_seq(ctx, "encoding");
      xml_parse_eq(ctx);
      src->decl_encoding = xml_parse_encoding_name(ctx);
      TRACE(ctx, "encoding=%s", src->decl_encoding);
      if (!xml_parse_white(ctx, 0))
	goto end;
    }
  else if (!doc)
    xml_error(ctx, "Expected XML encoding");

  /* Parse whether the document is standalone (optional in XMLDecl) */
  if (doc && xml_peek_char(ctx) == 's')
    {
      xml_parse_seq(ctx, "standalone");
      xml_parse_eq(ctx);
      uint c = xml_parse_quote(ctx);
      if (ctx->standalone = (xml_peek_char(ctx) == 'y'))
	xml_parse_seq(ctx, "yes");
      else
        xml_parse_seq(ctx, "no");
      xml_parse_char(ctx, c);
      TRACE(ctx, "standalone=%d", ctx->standalone);
      xml_parse_white(ctx, 0);
    }
end:
  xml_parse_seq(ctx, "?>");

  /* Switch to the final encoding */
  if (src->decl_encoding)
    {
      int cs = find_charset_by_name(src->decl_encoding);
      if (cs < 0 && !expected_encoding)
	xml_error(ctx, "Unknown encoding '%s'", src->decl_encoding);
      else if (!src->fb_encoding && cs >= 0 && cs != CONV_CHARSET_UTF8)
        {
	  xml_init_charconv(ctx, cs);
	  src->fb_encoding = src->decl_encoding;
	}
      else if (expected_encoding && strcasecmp(src->decl_encoding, expected_encoding) && (!utf16 ||
	!(!strcasecmp(src->decl_encoding, "UTF-16") ||
	 (!strcasecmp(src->decl_encoding, "UTF-16BE") && strcasecmp(expected_encoding, "UTF-16LE")) ||
	 (!strcasecmp(src->decl_encoding, "UTF-16LE") && strcasecmp(expected_encoding, "UTF-16BE")))))
	xml_error(ctx, "The header contains encoding '%s' instead of expected '%s'", src->decl_encoding, expected_encoding);
    }
  if (!src->fb_encoding)
    src->fb_encoding = "UTF-8";
  TRACE(ctx, "Final encoding=%s", src->fb_encoding);

exit:
  /* Update valid Unicode ranges */
  if (doc)
    xml_init_cats(ctx);
  src->refill_cat1 = ctx->cat_unrestricted & ~ctx->cat_new_line;
  src->refill_cat2 = ctx->cat_new_line;
}
