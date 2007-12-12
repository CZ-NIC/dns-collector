/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mempool.h"
#include "lib/fastbuf.h"
#include "lib/ff-unicode.h"
#include "lib/ff-binary.h"
#include "lib/chartype.h"
#include "lib/unicode.h"
#include "lib/hashfunc.h"
#include "lib/stkstring.h"
#include "lib/unaligned.h"
#include "charset/charconv.h"
#include "charset/fb-charconv.h"
#include "sherlock/xml/xml.h"
#include "sherlock/xml/dtd.h"
#include "sherlock/xml/common.h"

#include <setjmp.h>

/*** Error handling ***/

void NONRET
xml_throw(struct xml_context *ctx)
{
  ASSERT(ctx->err_code && ctx->throw_buf);
  longjmp(*(jmp_buf *)ctx->throw_buf, ctx->err_code);
}

void
xml_warn(struct xml_context *ctx, const char *format, ...)
{
  if (ctx->h_warn)
    {
      va_list args;
      va_start(args, format);
      ctx->err_msg = stk_vprintf(format, args);
      ctx->err_code = XML_ERR_WARN;
      va_end(args);
      ctx->h_warn(ctx);
      ctx->err_msg = NULL;
      ctx->err_code = XML_ERR_OK;
    }
}

void
xml_error(struct xml_context *ctx, const char *format, ...)
{
  if (ctx->h_error)
    {
      va_list args;
      va_start(args, format);
      ctx->err_msg = stk_vprintf(format, args);
      ctx->err_code = XML_ERR_ERROR;
      va_end(args);
      ctx->h_error(ctx);
      ctx->err_msg = NULL;
      ctx->err_code = XML_ERR_OK;
    }
}

void NONRET
xml_fatal(struct xml_context *ctx, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  ctx->err_msg = mp_vprintf(ctx->stack, format, args);
  ctx->err_code = XML_ERR_FATAL;
  ctx->state = XML_STATE_FATAL;
  va_end(args);
  if (ctx->h_fatal)
    ctx->h_fatal(ctx);
  xml_throw(ctx);
}

/*** Charecter categorization ***/

#include "obj/sherlock/xml/unicat.c"

/*** Memory management ***/

void NONRET
xml_fatal_nested(struct xml_context *ctx)
{
  xml_fatal(ctx, "Entity not nested correctly");
}

void *
xml_hash_new(struct mempool *pool, uns size)
{
  void *tab = mp_alloc_zero(pool, size + XML_HASH_HDR_SIZE);
  *(void **)tab = pool;
  return tab + XML_HASH_HDR_SIZE;
}

/*** Reading of document/external entities ***/

static void NONRET
xml_eof(struct xml_context *ctx)
{
  ctx->err_msg = "Unexpected EOF";
  ctx->err_code = XML_ERR_EOF;
  xml_throw(ctx);
}

static inline void
xml_add_char(u32 **bstop, uns c)
{
  *(*bstop)++ = c;
  *(*bstop)++ = xml_char_cat(c);
}

struct xml_source *
xml_push_source(struct xml_context *ctx, uns flags)
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
  ctx->flags = (ctx->flags & ~(XML_FLAG_SRC_EOF | XML_FLAG_SRC_EXPECTED_DECL | XML_FLAG_SRC_NEW_LINE | XML_FLAG_SRC_SURROUND | XML_FLAG_SRC_DOCUMENT)) | flags;
  ctx->bstop = ctx->bptr = src->buf;
  ctx->depth = 0;
  if (flags & XML_FLAG_SRC_SURROUND)
    xml_add_char(&ctx->bstop, 0x20);
  return src;
}

static void
xml_pop_source(struct xml_context *ctx)
{
  TRACE(ctx, "pop_source");
  if (unlikely(ctx->depth != 0))
    xml_fatal_nested(ctx);
  struct xml_source *src = ctx->src;
  ASSERT(src);
  bclose(src->fb);
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

static void xml_refill_utf8(struct xml_context *ctx);

void
xml_push_entity(struct xml_context *ctx, struct xml_dtd_ent *ent)
{
  TRACE(ctx, "xml_push_entity");
  uns cat1 = ctx->src->refill_cat1;
  uns cat2 = ctx->src->refill_cat2;
  struct xml_source *src = xml_push_source(ctx, 0);
  src->refill_cat1 = cat1;
  src->refill_cat2 = cat2;
  if (ent->flags & XML_DTD_ENT_EXTERNAL)
    xml_fatal(ctx, "External entities not implemented"); // FIXME
  else
    {
      fbbuf_init_read(src->fb = &src->wrap_fb, ent->text, ent->len, 0);
      src->refill = xml_refill_utf8;
    }
}

void
xml_set_source(struct xml_context *ctx, struct fastbuf *fb)
{
  TRACE(ctx, "xml_set_source");
  ASSERT(!ctx->src);
  struct xml_source *src = xml_push_source(ctx, XML_FLAG_SRC_DOCUMENT | XML_FLAG_SRC_EXPECTED_DECL);
  src->fb = fb;
}

static uns
xml_error_restricted(struct xml_context *ctx, uns c)
{
  if (c == ~1U)
    xml_error(ctx, "Corrupted encoding");
  else
    xml_error(ctx, "Restricted char U+%04X", c);
  return UNI_REPLACEMENT;
}

void xml_parse_decl(struct xml_context *ctx);

#define REFILL(ctx, func, params...)							\
  struct xml_source *src = ctx->src;							\
  struct fastbuf *fb = src->fb;								\
  if (ctx->bptr == ctx->bstop)								\
    ctx->bptr = ctx->bstop = src->buf;							\
  uns f = ctx->flags, c, t1 = src->refill_cat1, t2 = src->refill_cat2, row = src->row;	\
  u32 *bend = src->buf + ARRAY_SIZE(src->buf), *bstop = ctx->bstop,			\
      *last_0xd = (f & XML_FLAG_SRC_NEW_LINE) ? bstop : bend;				\
  do											\
    {											\
      c = func(fb, ##params);								\
      uns t = xml_char_cat(c);								\
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
	      last_0xd = bend;								\
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
	  if (f & XML_FLAG_SRC_SURROUND)						\
	    xml_add_char(&bstop, 0x20);							\
          f |= XML_FLAG_SRC_EOF;							\
          break;									\
	}										\
    }											\
  while (bstop < bend);									\
  ctx->flags = (last_0xd == bstop) ? f | XML_FLAG_SRC_NEW_LINE : f & ~XML_FLAG_SRC_NEW_LINE; \
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

#if 0
static inline uns
xml_refill_libcharset_bget(struct fastbuf *fb, unsigned short int *in_to_x)
{
  // FIXME: slow
  int c;
  return (unlikely(c = bgetc(fb) < 0)) ? c : (int)conv_x_to_ucs(in_to_x[c]);
}

static void
xml_refill_libcharset(struct xml_context *ctx)
{
  unsigned short int *in_to_x = ctx->src->refill_in_to_x;
  REFILL(ctx, xml_refill_libcharset_bget, in_to_x);
}
#endif

#undef REFILL

void
xml_refill(struct xml_context *ctx)
{
  do
    {
      if (ctx->flags & XML_FLAG_SRC_EOF)
	xml_pop_source(ctx);
      else if (ctx->flags & XML_FLAG_SRC_EXPECTED_DECL)
	xml_parse_decl(ctx);
      else
        {
	  ctx->src->refill(ctx);
	  TRACE(ctx, "refilled %u characters", (uns)((ctx->bstop - ctx->bptr) / 2));
	}
    }
  while (ctx->bptr == ctx->bstop);
}

uns
xml_row(struct xml_context *ctx)
{
  struct xml_source *src = ctx->src;
  if (!src)
    return 0;
  uns row = src->row;
  for (u32 *p = ctx->bstop; p != ctx->bptr; p -= 2)
    if (p[-1] & src->refill_cat2)
      row--;
  return row + 1;
}

/*** Basic parsing ***/

void NONRET
xml_fatal_expected(struct xml_context *ctx, uns c)
{
  xml_fatal(ctx, "Expected '%c'", c);
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

/* Names and nmtokens */

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
  return xml_parse_string(ctx, pool,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_SNAME_1_0 : XML_CHAR_SNAME_1_1,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_NAME_1_0 : XML_CHAR_NAME_1_1,
    "Expected a name");
}

void
xml_skip_name(struct xml_context *ctx)
{
  xml_skip_string(ctx,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_SNAME_1_0 : XML_CHAR_SNAME_1_1,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_NAME_1_0 : XML_CHAR_NAME_1_1,
    "Expected a name");
}

char *
xml_parse_nmtoken(struct xml_context *ctx, struct mempool *pool)
{
  /* Nmtoken ::= (NameChar)+ */
  uns cat = !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_NAME_1_0 : XML_CHAR_NAME_1_1;
  return xml_parse_string(ctx, pool, cat, cat, "Expected a nmtoken");
}

/* Simple literals */

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

static char *
xml_parse_encoding_name(struct xml_context *ctx)
{
  /* EncName ::= '"' [A-Za-z] ([A-Za-z0-9._] | '-')* '"' | "'" [A-Za-z] ([A-Za-z0-9._] | '-')* "'" */
  char *p = mp_start_noalign(ctx->pool, 1);
  uns q = xml_parse_quote(ctx);
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

/* Document/external entity header */

static inline void
xml_init_cats(struct xml_context *ctx, uns mask)
{
  if (!(ctx->flags & XML_FLAG_VERSION_1_1))
    {
      ctx->src->refill_cat1 = XML_CHAR_VALID_1_0 & ~XML_CHAR_NEW_LINE_1_0 & ~mask;
      ctx->src->refill_cat2 = XML_CHAR_NEW_LINE_1_0;
    }
  else
    {
      ctx->src->refill_cat1 = XML_CHAR_UNRESTRICTED_1_1 & ~XML_CHAR_NEW_LINE_1_1 & ~mask;
      ctx->src->refill_cat2 = XML_CHAR_NEW_LINE_1_1;
    }
}

static void
xml_init_charconv(struct xml_context *ctx, int cs)
{
  // FIXME: hack
  struct xml_source *src = ctx->src;
  TRACE(ctx, "wrapping charset %s", charset_name(cs));
#if 0
  struct conv_context conv;
  conv_set_charset(&conv, cs, CONV_CHARSET_UTF8);
  src->refill = xml_refill_libcharset;
  src->refill_in_to_x = conv.in_to_x;
#else
  src->fb = fb_wrap_charconv_in(src->fb, cs, CONV_CHARSET_UTF8);
  // FIXME: memory leak
#endif
}

void
xml_parse_decl(struct xml_context *ctx)
{
  TRACE(ctx, "xml_parse_decl");
  struct xml_source *src = ctx->src;
  ctx->flags &= ~XML_FLAG_SRC_EXPECTED_DECL;

  /* Setup valid Unicode ranges and force the reader to abort refill() after each '>', where we can switch encoding or XML version */
  xml_init_cats(ctx, XML_CHAR_GT);

  /* Initialize the supplied charset (if any) or try to guess it */
  char *expected_encoding = src->expected_encoding ? : src->fb_encoding;
  src->refill = xml_refill_utf8;
  int bom = bpeekc(src->fb);
  if (bom < 0)
    ctx->flags |= XML_FLAG_SRC_EOF;
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
	  if (!src->expected_encoding)
	    expected_encoding = (bom == 0xff) ? "UTF-16LE" : "UTF-16BE";
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
  uns utf16 = src->refill == xml_refill_utf16_le || src->refill == xml_refill_utf16_be;
  if (bom > 0 && xml_peek_char(ctx) == 0xfeff)
    xml_skip_char(ctx);
  else if (utf16)
    xml_error(ctx, "Missing or corrupted BOM");

  /* Look ahead for presence of XMLDecl or optional TextDecl */
  if (!(ctx->flags & XML_FLAG_SRC_EOF) && ctx->bstop != src->buf + ARRAY_SIZE(src->buf))
    xml_refill(ctx);
  uns doc = ctx->flags & XML_FLAG_SRC_DOCUMENT;
  u32 *bptr = ctx->bptr;
  uns have_decl = (12 <= ctx->bstop - ctx->bptr && (bptr[11] & XML_CHAR_WHITE) &&
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
      uns v = 0;
      if (!strcmp(version, "1.1"))
	v = XML_FLAG_VERSION_1_1;
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
      else if (v > (ctx->flags & XML_FLAG_VERSION_1_1))
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
      uns c = xml_parse_quote(ctx);
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
	xml_init_charconv(ctx, cs);
      else if (expected_encoding && strcasecmp(src->decl_encoding, expected_encoding) && (!utf16 ||
	!(!strcasecmp(src->decl_encoding, "UTF-16") ||
	 (!strcasecmp(src->decl_encoding, "UTF-16BE") && strcasecmp(expected_encoding, "UTF-16LE")) ||
	 (!strcasecmp(src->decl_encoding, "UTF-16LE") && strcasecmp(expected_encoding, "UTF-16BE")))))
	xml_error(ctx, "The header contains encoding '%s' instead of expected '%s'", src->decl_encoding, expected_encoding);
    }

exit:
  /* Update valid Unicode ranges */
  xml_init_cats(ctx, 0);
}
