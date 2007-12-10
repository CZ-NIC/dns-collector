/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/* TODO:
 * - iface
 * - stack-like memory handling where possible
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

#include <setjmp.h>

/*** Debugging ***/

#ifdef LOCAL_DEBUG
#define TRACE(c, f, p...) do { DBG("XML %u: " f, xml_row(c), ##p); } while(0)
#else
#define TRACE(c, f, p...) do {} while(0)
#endif

static uns xml_row(struct xml_context *ctx);

/*** Error handling ***/

static void NONRET
xml_throw(struct xml_context *ctx)
{
  ASSERT(ctx->err_code && ctx->throw_buf);
  longjmp(*(jmp_buf *)ctx->throw_buf, ctx->err_code);
}

static void
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

static void
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

static void NONRET
xml_fatal(struct xml_context *ctx, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  ctx->err_msg = mp_vprintf(ctx->pool, format, args);
  ctx->err_code = XML_ERR_FATAL;
  ctx->state = XML_STATE_FATAL;
  va_end(args);
  if (ctx->h_fatal)
    ctx->h_fatal(ctx);
  xml_throw(ctx);
}

/*** Charecter categorization ***/

#include "obj/sherlock/xml/xml-ucat.h"

static inline uns
xml_char_cat(uns c)
{
  if (c < 0x10000)
    return 1U << xml_char_tab2[(c & 0xff) + xml_char_tab1[c >> 8]];
  else if (likely(c < 0x110000))
    return 1U << xml_char_tab3[c >> 16];
  else
    return 1;
}

/*** Memory management ***/

static void NONRET
xml_fatal_nested(struct xml_context *ctx)
{
  xml_fatal(ctx, "Entity not nested correctly");
}

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

static inline void
xml_push(struct xml_context *ctx)
{
  TRACE(ctx, "push");
  struct xml_stack *s = mp_alloc(ctx->pool, sizeof(*s));
  mp_save(ctx->pool, &s->saved_pool);
  s->saved_flags = ctx->flags;
  s->next = ctx->stack;
  ctx->stack = s;
  xml_inc(ctx);
}

static inline void
xml_pop(struct xml_context *ctx)
{
  TRACE(ctx, "pop");
  xml_dec(ctx);
  struct xml_stack *s = ctx->stack;
  ASSERT(s);
  ctx->stack = s->next;
  ctx->flags = s->saved_flags;
  mp_restore(ctx->pool, &s->saved_pool);
}

#define XML_HASH_HDR_SIZE ALIGN_TO(sizeof(void *), CPU_STRUCT_ALIGN)
#define XML_HASH_GIVE_ALLOC struct HASH_PREFIX(table); \
  static inline void *HASH_PREFIX(alloc)(struct HASH_PREFIX(table) *t, uns size) \
  { return mp_alloc(*(void **)((void *)t - XML_HASH_HDR_SIZE), size); } \
  static inline void HASH_PREFIX(free)(struct HASH_PREFIX(table) *t UNUSED, void *p UNUSED) {}

static void *
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

static struct xml_source *
xml_push_source(struct xml_context *ctx, uns flags)
{
  xml_push(ctx);
  struct xml_source *src = ctx->src;
  if (src)
    {
      src->bptr = ctx->bptr;
      src->bstop = ctx->bstop;
    }
  src = mp_alloc_zero(ctx->pool, sizeof(*src));
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
  TRACE(ctx, "xml_pop_source");
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

static void
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

static void xml_parse_decl(struct xml_context *ctx);

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

static void
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

static uns
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

static void NONRET
xml_fatal_expected(struct xml_context *ctx, uns c)
{
  xml_fatal(ctx, "Expected '%c'", c);
}

static void NONRET
xml_fatal_expected_white(struct xml_context *ctx)
{
  xml_fatal(ctx, "Expected a white space");
}

static void NONRET
xml_fatal_expected_quot(struct xml_context *ctx)
{
  xml_fatal(ctx, "Expected a quotation mark");
}

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

static void
xml_parse_eq(struct xml_context *ctx)
{
  /* Eq ::= S? '=' S? */
  xml_parse_white(ctx, 0);
  xml_parse_char(ctx, '=');
  xml_parse_white(ctx, 0);
}

static inline uns
xml_parse_quote(struct xml_context *ctx)
{
  /* "'" | '"' */
  uns c = xml_get_char(ctx);
  if (unlikely(c != '\'' && c != '\"'))
    xml_fatal_expected_quot(ctx);
  return c;
}

/* Names and nmtokens */

static char *
xml_parse_string(struct xml_context *ctx, uns first_cat, uns next_cat, char *err)
{
  char *p = mp_start_noalign(ctx->pool, 1);
  if (unlikely(!(xml_peek_cat(ctx) & first_cat)))
    xml_fatal(ctx, "%s", err);
  do
    {
      p = mp_spread(ctx->pool, p, 5);
      p = utf8_32_put(p, xml_skip_char(ctx));
    }
  while (xml_peek_cat(ctx) & next_cat);
  *p++ = 0;
  return mp_end(ctx->pool, p);
}

static void
xml_skip_string(struct xml_context *ctx, uns first_cat, uns next_cat, char *err)
{
  if (unlikely(!(xml_get_cat(ctx) & first_cat)))
    xml_fatal(ctx, "%s", err);
  while (xml_peek_cat(ctx) & next_cat)
    xml_skip_char(ctx);
}

static char *
xml_parse_name(struct xml_context *ctx)
{
  /* Name ::= NameStartChar (NameChar)* */
  return xml_parse_string(ctx,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_SNAME_1_0 : XML_CHAR_SNAME_1_1,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_NAME_1_0 : XML_CHAR_NAME_1_1,
    "Expected a name");
}

static void
xml_skip_name(struct xml_context *ctx)
{
  xml_skip_string(ctx,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_SNAME_1_0 : XML_CHAR_SNAME_1_1,
    !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_NAME_1_0 : XML_CHAR_NAME_1_1,
    "Expected a name");
}

static char *
xml_parse_nmtoken(struct xml_context *ctx)
{
  /* Nmtoken ::= (NameChar)+ */
  uns cat = !(ctx->flags & XML_FLAG_VERSION_1_1) ? XML_CHAR_NAME_1_0 : XML_CHAR_NAME_1_1;
  return xml_parse_string(ctx, cat, cat, "Expected a nmtoken");
}

/* Simple literals */

static char *
xml_parse_system_literal(struct xml_context *ctx)
{
  /* SystemLiteral ::= ('"' [^"]* '"') | ("'" [^']* "'") */
  char *p = mp_start_noalign(ctx->pool, 1);
  uns q = xml_parse_quote(ctx), c;
  while ((c = xml_get_char(ctx)) != q)
    {
      p = mp_spread(ctx->pool, p, 5);
      p = utf8_32_put(p, c);
    }
  *p++ = 0;
  return mp_end(ctx->pool, p);
}

static char *
xml_parse_pubid_literal(struct xml_context *ctx)
{
  /* PubidLiteral ::= '"' PubidChar* '"' | "'" (PubidChar - "'")* "'" */
  char *p = mp_start_noalign(ctx->pool, 1);
  uns q = xml_parse_quote(ctx), c;
  while ((c = xml_get_char(ctx)) != q)
    {
      if (unlikely(!(xml_last_cat(ctx) & XML_CHAR_PUBID)))
	xml_fatal(ctx, "Expected a pubid character");
      p = mp_spread(ctx->pool, p, 2);
      *p++ = c;
    }
  *p++ = 0;
  return mp_end(ctx->pool, p);
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

static void
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
      char *version = xml_parse_pubid_literal(ctx);
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

/*** Document Type Definition (DTD) ***/

/* Notations */

#define HASH_PREFIX(x) xml_dtd_notns_##x
#define HASH_NODE struct xml_dtd_notn
#define HASH_KEY_STRING name
#define HASH_ZERO_FILL
#define HASH_TABLE_DYNAMIC
#define HASH_WANT_FIND
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
xml_dtd_declare_trivial_gent(struct xml_context *ctx, char *name, char *text)
{
  struct xml_dtd *dtd = ctx->dtd;
  struct xml_dtd_ent *ent = xml_dtd_ents_lookup(dtd->tab_gents, name);
  if (ent->flags & XML_DTD_ENT_DECLARED)
    {
      xml_warn(ctx, "Entity &%s; already declared", name);
      return NULL;
    }
  slist_add_tail(&dtd->gents, &ent->n);
  ent->flags = XML_DTD_ENT_DECLARED | XML_DTD_ENT_TRIVIAL;
  ent->text = text;
  ent->len = strlen(text);
  return ent;
}

static void
xml_dtd_declare_default_gents(struct xml_context *ctx)
{
  xml_dtd_declare_trivial_gent(ctx, "lt", "<");
  xml_dtd_declare_trivial_gent(ctx, "gt", ">");
  xml_dtd_declare_trivial_gent(ctx, "amp", "&");
  xml_dtd_declare_trivial_gent(ctx, "apos", "'");
  xml_dtd_declare_trivial_gent(ctx, "quot", "\"");
}

static struct xml_dtd_ent *
xml_dtd_find_gent(struct xml_context *ctx, char *name)
{
  struct xml_dtd *dtd = ctx->dtd;
  if (dtd)
    {
      struct xml_dtd_ent *ent = xml_dtd_ents_find(dtd->tab_gents, name);
      return !ent ? NULL : (ent->flags & XML_DTD_ENT_DECLARED) ? ent : NULL;
    }
  else
    {
#define ENT(n, t) ent_##n = { .name = #n, .text = t, .len = 1, .flags = XML_DTD_ENT_DECLARED | XML_DTD_ENT_TRIVIAL }
      static struct xml_dtd_ent ENT(lt, "<"), ENT(gt, ">"), ENT(amp, "&"), ENT(apos, "'"), ENT(quot, "\"");
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
#define HASH_WANT_LOOKUP
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

static void
xml_dtd_init(struct xml_context *ctx)
{
  if (ctx->dtd)
    return;
  struct mempool *pool = mp_new(4096);
  struct xml_dtd *dtd = ctx->dtd = mp_alloc_zero(pool, sizeof(*ctx->dtd));
  dtd->pool = pool;
  xml_dtd_ents_init(dtd->tab_gents = xml_hash_new(pool, sizeof(struct xml_dtd_ents_table)));
  xml_dtd_ents_init(dtd->tab_pents = xml_hash_new(pool, sizeof(struct xml_dtd_ents_table)));
  xml_dtd_notns_init(dtd->tab_notns = xml_hash_new(pool, sizeof(struct xml_dtd_notns_table)));
  xml_dtd_elems_init(dtd->tab_elems = xml_hash_new(pool, sizeof(struct xml_dtd_elems_table)));
  xml_dtd_attrs_init(dtd->tab_attrs = xml_hash_new(pool, sizeof(struct xml_dtd_attrs_table)));
  xml_dtd_evals_init(dtd->tab_evals = xml_hash_new(pool, sizeof(struct xml_dtd_evals_table)));
  xml_dtd_enotns_init(dtd->tab_enotns = xml_hash_new(pool, sizeof(struct xml_dtd_enotns_table)));
  xml_dtd_declare_default_gents(ctx);
}

static void
xml_dtd_cleanup(struct xml_context *ctx)
{
  if (!ctx->dtd)
    return;
  mp_delete(ctx->dtd->pool);
  ctx->dtd = NULL;
}

static void
xml_dtd_finish(struct xml_context *ctx)
{
  if (!ctx->dtd)
    return;
  // FIXME
}

/*** Parsing functions ***/

/* Comments */

static void
xml_push_comment(struct xml_context *ctx)
{
  /* Parse a comment to ctx->value:
   *   Comment ::= '<!--' ((Char - '-') | ('-' (Char - '-')))* '-->'
   * Already parsed: '<!-' */
  struct fastbuf *out = ctx->value;
  uns c;
  xml_parse_char(ctx, '-');
  while (1)
    {
      if ((c = xml_get_char(ctx)) == '-')
	if ((c = xml_get_char(ctx)) == '-')
	  break;
	else
	  bputc(out, '-');
      bput_utf8_32(out, c);
    }
  xml_parse_char(ctx, '>');
  xml_dec(ctx);
  fbgrow_rewind(out);
  if (ctx->h_comment)
    ctx->h_comment(ctx);
}

static void
xml_pop_comment(struct xml_context *ctx)
{
  fbgrow_rewind(ctx->value);
}

static void
xml_skip_comment(struct xml_context *ctx)
{
  xml_parse_char(ctx, '-');
  while (xml_get_char(ctx) != '-' || xml_get_char(ctx) != '-');
  xml_parse_char(ctx, '>');
  xml_dec(ctx);
}

/* Processing instructions */

static void
xml_push_pi(struct xml_context *ctx)
{
  /* Parses a PI to ctx->value and ctx->name:
   *   PI       ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
   *   PITarget ::= Name - (('X' | 'x') ('M' | 'm') ('L' | 'l'))
   * Already parsed: '<?' */

  ctx->name = xml_parse_name(ctx);
  if (unlikely(!strcasecmp(ctx->name, "xml")))
    xml_fatal(ctx, "Reserved PI target");
  struct fastbuf *out = ctx->value;
  if (xml_parse_white(ctx, 0))
    xml_parse_seq(ctx, "?>");
  else
    {
      while (1)
        {
	  uns c;
	  if ((c = xml_get_char(ctx)) == '?')
	    if (xml_get_char(ctx) == '>')
	      break;
	    else
	      {
	        xml_unget_char(ctx);
		bputc(out, '?');
	      }
	  else
	    bput_utf8_32(out, c);
	}
      fbgrow_rewind(out);
    }
  xml_dec(ctx);
  if (ctx->h_pi)
    ctx->h_pi(ctx);
}

static void
xml_pop_pi(struct xml_context *ctx)
{
  fbgrow_reset(ctx->value);
}

static void
xml_skip_pi(struct xml_context *ctx)
{
  if (ctx->flags & XML_FLAG_VALIDATING)
    {
      mp_push(ctx->pool);
      if (unlikely(!strcasecmp(xml_parse_name(ctx), "xml")))
	xml_fatal(ctx, "Reserved PI target");
      mp_pop(ctx->pool);
      if (!xml_parse_white(ctx, 0))
        {
	  xml_parse_seq(ctx, "?>");
	  xml_dec(ctx);
	  return;
	}
    }
  while (1)
    if (xml_get_char(ctx) == '?')
      if (xml_get_char(ctx) == '>')
	break;
      else
	xml_unget_char(ctx);
  xml_dec(ctx);
}

/* Character references */

static uns
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

/* References to general entities */

static void
xml_parse_ge_ref(struct xml_context *ctx, struct fastbuf *out)
{
  /* Reference ::= EntityRef | CharRef
   * EntityRef ::= '&' Name ';'
   * Already parsed: '&' */
  if (xml_peek_char(ctx) == '#')
    {
      xml_skip_char(ctx);
      uns c = xml_parse_char_ref(ctx);
      bput_utf8_32(out, c);
    }
  else
    {
      struct mempool_state state;
      mp_save(ctx->pool, &state);
      char *name = xml_parse_name(ctx);
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
	  mp_restore(ctx->pool, &state);
          xml_dec(ctx);
	  xml_push_entity(ctx, ent);
	  return;
	}
      mp_restore(ctx->pool, &state);
      xml_dec(ctx);
    }
}

/* References to parameter entities */

static void
xml_parse_pe_ref(struct xml_context *ctx)
{
  /* PEReference ::= '%' Name ';'
   * Already parsed: '%' */
  struct mempool_state state;
  mp_save(ctx->pool, &state);
  char *name = xml_parse_name(ctx);
  xml_parse_char(ctx, ';');
  struct xml_dtd_ent *ent = xml_dtd_find_pent(ctx, name);
  if (!ent)
    xml_error(ctx, "Unknown entity %%%s;", name);
  else
    {
      TRACE(ctx, "Pushed entity %%%s;", name);
      mp_restore(ctx->pool, &state);
      xml_dec(ctx);
      xml_push_entity(ctx, ent);
      return;
    }
  mp_restore(ctx->pool, &state);
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

static inline uns
xml_check_dtd_pe(struct xml_context *ctx)
{
  if (xml_peek_char(ctx) == '%')
    {
      xml_parse_dtd_pe(ctx);
      return 1;
    }
  return 0;
}

/* External ID */

static void
xml_parse_external_id(struct xml_context *ctx, struct xml_ext_id *eid, uns allow_public, uns dtd)
{
  bzero(eid, sizeof(*eid));
  if (dtd)
    xml_check_dtd_pe(ctx);
  uns c = xml_peek_char(ctx);
  if (c == 'S')
    {
      xml_parse_seq(ctx, "SYSTEM");
      if (dtd)
	xml_parse_dtd_white(ctx, 1);
      else
	xml_parse_white(ctx, 1);
      eid->system_id = xml_parse_system_literal(ctx);
    }
  else if (c == 'P')
    {
      xml_parse_seq(ctx, "PUBLIC");
      if (dtd)
	xml_parse_dtd_white(ctx, 1);
      else
	xml_parse_white(ctx, 1);
      eid->public_id = xml_parse_pubid_literal(ctx);
      if (dtd ? xml_parse_dtd_white(ctx, 0) : xml_parse_white(ctx, 0))
	if ((c = xml_peek_char(ctx)) == '\'' || c == '"' || !allow_public)
          eid->system_id = xml_parse_system_literal(ctx);
    }
  else
    xml_fatal(ctx, "Expected an external ID");
}

/* DTD: Notation declaration */

static void
xml_parse_notation_decl(struct xml_context *ctx)
{
  /* NotationDecl ::= '<!NOTATION' S Name S (ExternalID | PublicID) S? '>'
   * Already parsed: '<!NOTATION' */
  TRACE(ctx, "parse_notation_decl");
  struct xml_dtd *dtd = ctx->dtd;
  xml_parse_dtd_white(ctx, 1);

  struct xml_dtd_notn *notn = xml_dtd_notns_lookup(dtd->tab_notns, xml_parse_name(ctx));
  xml_parse_dtd_white(ctx, 1);
  struct xml_ext_id eid;
  xml_parse_external_id(ctx, &eid, 1, 1);
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

static void
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

  struct xml_dtd_ent *ent = xml_dtd_ents_lookup(flags ? dtd->tab_pents : dtd->tab_gents, xml_parse_name(ctx));
  slist *list = flags ? &dtd->pents : &dtd->gents;
  xml_parse_white(ctx, 1);
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
      struct fastbuf *out = ctx->value;
      while (1)
        {
	  if ((c = xml_get_char(ctx)) == sep)
	    break;
	  else if (c == '%')
	    {
	      // FIXME
	      ASSERT(0);
	      //xml_parse_parameter_ref(ctx);
	    }
	  else if (c != '&')
	    bput_utf8_32(out, c);
	  else if ((c = xml_get_char(ctx)) == '#')
	    c = xml_parse_char_ref(ctx);
	  else
	    {
	      /* Bypass references to general entities */
	      mp_push(ctx->pool);
	      bputc(out, '&');
	      xml_unget_char(ctx);
	      bputs(out, xml_parse_name(ctx));
	      xml_parse_char(ctx, ';');
	      bputc(out, ';');
	      mp_pop(ctx->pool);
	    }
	}
      bputc(out, 0);
      fbgrow_rewind(out);
      slist_add_tail(list, &ent->n);
      ent->flags = flags | XML_DTD_ENT_DECLARED;
      ent->len = out->bstop - out->bptr - 1;
      ent->text = mp_memdup(ctx->pool, out->bptr, ent->len + 1);
      fbgrow_reset(out);
    }
  else
    {
      /* External entity */
      struct xml_ext_id eid;
      struct xml_dtd_notn *notn = NULL;
      xml_parse_external_id(ctx, &eid, 0, 0);
      if (!xml_parse_white(ctx, 0) || !flags)
	xml_parse_char(ctx, '>');
      else if (xml_get_char(ctx) != '>')
        {
	  /* General external unparsed entity */
	  flags |= XML_DTD_ENT_UNPARSED;
	  xml_parse_seq(ctx, "NDATA");
	  xml_parse_white(ctx, 1);
	  notn = xml_dtd_notns_lookup(dtd->tab_notns, xml_parse_name(ctx));
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
		    // FIXME: Element
		  }
		else
		  goto invalid_markup;
		break;
	      case 'A':
		xml_parse_seq(ctx, "TTLIST");
		// FIXME: AttList
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
	xml_parse_dtd_pe(ctx);
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
xml_parse_cdata(struct xml_context *ctx)
{
  struct fastbuf *out = ctx->chars;
  xml_parse_seq(ctx, "CDATA[");
  while (1)
    {
      uns c;
      if ((c = xml_get_char(ctx)) == ']')
        {
          if ((c = xml_get_char(ctx)) == ']')
	    if ((c = xml_get_char(ctx)) == '>')
	      break;
	    else
	      bputc(out, ']');
	  bputc(out, ']');
	}
      bput_utf8_32(out, c);
    }
}

static void
xml_skip_cdata(struct xml_context *ctx)
{
  xml_parse_cdata(ctx);
}

static void
xml_parse_chars(struct xml_context *ctx)
{
  TRACE(ctx, "parse_chars");
  struct fastbuf *out = ctx->chars;
  uns c;
  while ((c = xml_get_char(ctx)) != '<')
    if (c == '&')
      {
	xml_inc(ctx);
        xml_parse_ge_ref(ctx, out);
      }
    else
      bput_utf8_32(out, c);
  xml_unget_char(ctx);
}

/*----------------------------------------------*/

struct xml_attrs_table;

static inline uns
xml_attrs_hash(struct xml_attrs_table *t UNUSED, struct xml_elem *e, char *n)
{
  return hash_pointer(e) ^ hash_string(n);
}

static inline int
xml_attrs_eq(struct xml_attrs_table *t UNUSED, struct xml_elem *e1, char *n1, struct xml_elem *e2, char *n2)
{
  return (e1 == e2) && !strcmp(n1, n2);
}

static inline void
xml_attrs_init_key(struct xml_attrs_table *t UNUSED, struct xml_attr *a, struct xml_elem *e, char *name)
{
  a->elem = e;
  a->name = name;
  a->val = NULL;
  slist_add_tail(&e->attrs, &a->n);
}

#define HASH_PREFIX(x) xml_attrs_##x
#define HASH_NODE struct xml_attr
#define HASH_KEY_COMPLEX(x) x elem, x name
#define HASH_KEY_DECL struct xml_elem *elem, char *name
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

void
xml_init(struct xml_context *ctx)
{
  bzero(ctx, sizeof(*ctx));
  ctx->pool = mp_new(65536);
  ctx->chars = fbgrow_create(4096);
  ctx->value = fbgrow_create(4096);
  xml_dtd_init(ctx);
  xml_attrs_init(ctx->tab_attrs = xml_hash_new(ctx->pool, sizeof(struct xml_attrs_table)));
}

void
xml_cleanup(struct xml_context *ctx)
{
  xml_attrs_cleanup(ctx->tab_attrs);
  xml_dtd_cleanup(ctx);
  bclose(ctx->value);
  bclose(ctx->chars);
  mp_delete(ctx->pool);
}

static void
xml_parse_attr(struct xml_context *ctx)
{
  // FIXME: memory management, dtd, literal
  TRACE(ctx, "parse_attr");
  struct xml_elem *e = ctx->elem;
  char *name = xml_parse_name(ctx);
  struct xml_attr *a = xml_attrs_lookup(ctx->tab_attrs, e, name);
  xml_parse_eq(ctx);
  char *val =xml_parse_system_literal(ctx);
  if (a->val)
    xml_error(ctx, "Attribute is not unique");
  else
    a->val = val;
}

static void
xml_parse_stag(struct xml_context *ctx)
{
  // FIXME: dtd
  TRACE(ctx, "parse_stag");
  xml_push(ctx);
  struct xml_elem *e = mp_alloc_zero(ctx->pool, sizeof(*e));
  struct xml_elem *parent = ctx->elem;
  clist_init(&e->sons);
  e->node.parent = (void *)parent;
  ctx->elem = e;
  e->name = xml_parse_name(ctx);
  if (parent)
    clist_add_tail(&parent->sons, &e->node.n);
  else
    {
      ctx->root = e;
      if (ctx->document_type && strcmp(e->name, ctx->document_type))
	xml_error(ctx, "The root element does not match the document type");
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
xml_parse_etag(struct xml_context *ctx)
{
  TRACE(ctx, "parse_etag");
  struct xml_elem *e = ctx->elem;
  ASSERT(e);
  char *name = xml_parse_name(ctx);
  if (strcmp(name, e->name))
    xml_fatal(ctx, "Invalid ETag, expected '%s'", e->name);
  xml_parse_white(ctx, 0);
  xml_parse_char(ctx, '>');
  xml_dec(ctx);
}

static void
xml_pop_element(struct xml_context *ctx)
{
  TRACE(ctx, "pop_element");
  if (ctx->h_element_end)
    ctx->h_element_end(ctx);
  struct xml_elem *e = ctx->elem;
  if (ctx->flags & XML_DOM_FREE)
    {
      if (e->node.parent)
        clist_remove(&e->node.n);
      else
	ctx->root = NULL;
      SLIST_FOR_EACH(struct xml_attr *, a, e->attrs)
	xml_attrs_remove(ctx->tab_attrs, a);
      struct xml_node *n;
      while (n = clist_head(&e->sons))
        {
	  if (n->type == XML_NODE_ELEM)
	    {
	      SLIST_FOR_EACH(struct xml_attr *, a, ((struct xml_elem *)n)->attrs)
		xml_attrs_remove(ctx->tab_attrs, a);
	      clist_insert_list_after(&((struct xml_elem *)n)->sons, &n->n);
	    }
	  clist_remove(&n->n);
	}
    }
  ctx->node = e->node.parent;
  xml_pop(ctx); // FIXME: memory management without XML_DOM_FREE
  xml_dec(ctx);
#if 0
  for (struct xml_attribute *a = e->attrs; a; a = a->next)
    xml_attribute_remove(ctx->attribute_table, a);
#endif
}

static void
xml_parse_element_decl(struct xml_context *ctx)
{
  // FIXME
  mp_push(ctx->pool);
  xml_parse_seq(ctx, "<!ELEMENT");
  xml_parse_white(ctx, 1);
  xml_parse_name(ctx);
  xml_parse_white(ctx, 1);

  uns c = xml_get_char(ctx);
  if (c == 'E')
    {
      xml_parse_seq(ctx, "MPTY");
      // FIXME
    }
  else if (c == 'A')
    {
      xml_parse_seq(ctx, "NY");
      // FIXME
    }
  else if (c == '(')
    {
      xml_parse_white(ctx, 0);
      if (xml_get_char(ctx) == '#')
        {
	  xml_parse_seq(ctx, "PCDATA");
	  while (1)
	    {
	      xml_parse_white(ctx, 0);
	      if ((c = xml_get_char(ctx)) == ')')
		break;
	      else if (c != '|')
		xml_fatal_expected(ctx, ')');
	      xml_parse_white(ctx, 0);
	      xml_parse_name(ctx);
	      // FIXME
	    }
	}
      else
        {
	  xml_unget_char(ctx);
	  uns depth = 1;
	  while (1)
	    {
	      xml_parse_white(ctx, 0);
	      if ((c = xml_get_char(ctx)) == '(')
	        {
		  depth++;
		}
	      else if (c == ')')
	        {
		  if ((c = xml_get_char(ctx)) == '?' || c == '*' || c == '+')
		    {
		    }
		  else
		    xml_unget_char(ctx);
		  if (!--depth)
		    break;
		}
	      else if (c == '|')
	        {
		}
	      else if (c == ',')
	        {
		}
	      else
	        {
		  xml_unget_char(ctx);
		  xml_parse_name(ctx);
		}
	    }
	}
    }
  else
    xml_fatal(ctx, "Expected element content specification");

  xml_parse_white(ctx, 0);
  xml_parse_char(ctx, '>');
  mp_pop(ctx->pool);
}

#if 0
static void
xml_parse_attr_list_decl(struct xml_context *ctx)
{
  /* AttlistDecl ::= '<!ATTLIST' S Name AttDef* S? '>'
   * AttDef ::= S Name S AttType S DefaultDecl */
  xml_parse_seq(ctx, "ATTLIST");
  xml_parse_white(ctx, 1);
  struct xml_dtd_elem *e = xml_dtd_elems_lookup(ctx->dtd->tab_elems, xml_parse_name(ctx));
  e->attlist_declared = 1;

  while (xml_parse_white(ctx, 0) && xml_get_char(ctx) != '>')
    {
      xml_unget_char(ctx);
      char *name = xml_parse_name(ctx);
      struct xml_dtd_attr *a = xml_dtd_attrs_find(ctx->dtd->tab_attrs, e, name);
      uns ignored = 0;
      if (a)
        {
	  xml_warn(ctx, "Duplicate attribute definition");
	  ignored++;
	}
      else
	a = xml_dtd_attrs_new(ctx->dtd->tab_attrs, e, name);
      xml_parse_white(ctx, 1);
      if (xml_get_char(ctx) == '(')
        {
	  if (!ignored)
	    a->type = XML_ATTR_ENUM;
	  do
	    {
	      xml_parse_white(ctx, 0);
	      char *value = xml_parse_nmtoken(ctx);
	      if (!ignored)
		if (xml_dtd_evals_find(ctx->dtd->tab_evals, a, value))
		  xml_error(ctx, "Duplicate enumeration value");
	        else
		  xml_dtd_evals_new(ctx->dtd->tab_evals, a, value);
	      xml_parse_white(ctx, 0);
	    }
	  while (xml_get_char(ctx) == '|');
	  xml_unget_char(ctx);
	  xml_parse_char(ctx, ')');
	}
      else
        {
	  xml_unget_char(ctx);
	  char *type = xml_parse_name(ctx);
	  enum xml_dtd_attribute_type t;
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
	      t = XML_ATTR_NOTATION;
	      xml_parse_white(ctx, 1);
	      xml_parse_char(ctx, '(');
	      do
	        {
		  xml_parse_white(ctx, 0);
		  struct xml_dtd_notn *n = xml_dtd_notns_lookup(ctx->dtd->tab_notns, xml_parse_name(ctx));
		  if (!ignored)
		    if (xml_dtd_enotns_find(ctx->dtd->tab_enotns, a, n))
		      xml_error(ctx, "Duplicate enumerated notation");
		    else
		      xml_dtd_enotns_new(ctx->dtd->tab_enotns, a, n);
		  xml_parse_white(ctx, 0);
		}
	      while (xml_get_char(ctx) == '|');
	      xml_unget_char(ctx);
	      xml_parse_char(ctx, ')');
	    }
	  else
	    xml_fatal(ctx, "Unknown attribute type");
	  if (!ignored)
	    a->type = t;
	}
      xml_parse_white(ctx, 1);
      enum xml_dtd_attribute_default def = XML_ATTR_NONE;
      if (xml_get_char(ctx) == '#')
	switch (xml_get_char(ctx))
          {
	    case 'R':
	      xml_parse_seq(ctx, "EQUIRED");
	      def = XML_ATTR_REQUIRED;
	      break;
	    case 'I':
	      xml_parse_seq(ctx, "MPLIED");
	      def = XML_ATTR_IMPLIED;
	      break;
	    case 'F':
	      xml_parse_seq(ctx, "IXED");
	      def = XML_ATTR_FIXED;
	      break;
	    default:
	      xml_fatal(ctx, "Expected a modifier for default attribute value");
	  }
      else
	xml_unget_char(ctx);
      if (def != XML_ATTR_REQUIRED && def != XML_ATTR_IMPLIED)
        {
	  xml_parse_system_literal(ctx);
	  // FIXME
	}
    }
}
#endif

static void
xml_parse_doctype_decl(struct xml_context *ctx)
{
  if (ctx->document_type)
    xml_fatal(ctx, "Multiple document types not allowed");
  xml_parse_seq(ctx, "DOCTYPE");
  xml_parse_white(ctx, 1);
  ctx->document_type = xml_parse_name(ctx);
  TRACE(ctx, "doctyype=%s", ctx->document_type);
  uns white = xml_parse_white(ctx, 0);
  uns c = xml_peek_char(ctx);
  if (c != '>' && c != '[' && white)
    {
      xml_parse_external_id(ctx, &ctx->eid, 0, 0);
      xml_parse_white(ctx, 0);
      ctx->flags |= XML_FLAG_HAS_EXTERNAL_SUBSET;
    }
  if (xml_peek_char(ctx) == '[')
    ctx->flags |= XML_FLAG_HAS_INTERNAL_SUBSET;
  if (ctx->h_doctype_decl)
    ctx->h_doctype_decl(ctx);
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

      case XML_STATE_PI:
	mp_pop(ctx->pool);
      case XML_STATE_COMMENT:
	fbgrow_reset(ctx->value);

      case XML_STATE_CHARS:

	while (1)
	  {
	    if (xml_peek_char(ctx) != '<')
	      {
		/* CharData */
	        xml_parse_chars(ctx);
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
		    if (btell(ctx->chars))
		      {
			fbgrow_rewind(ctx->chars);
			ctx->state = XML_STATE_CHARS_BEFORE_PI;
			return XML_STATE_PI;
      case XML_STATE_CHARS_BEFORE_PI:
			fbgrow_reset(ctx->chars);
		      }
		    xml_push_pi(ctx);
		    return ctx->state = XML_STATE_PI;
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
		      if (btell(ctx->chars))
		        {
			  fbgrow_rewind(ctx->chars);
			  ctx->state = XML_STATE_CHARS_BEFORE_COMMENT;
			  return XML_STATE_CHARS;
      case XML_STATE_CHARS_BEFORE_COMMENT:
			  fbgrow_reset(ctx->chars);
			}
		      xml_push_comment(ctx);
		      return ctx->state = XML_STATE_COMMENT;
		    }
		}
	      else if (c == '[')
	        {
		  /* CDATA */
		  if (!(ctx->want & XML_WANT_CDATA))
		    xml_skip_cdata(ctx);
		  else
		    {
		      if (btell(ctx->chars))
		        {
			  fbgrow_rewind(ctx->chars);
			  ctx->state = XML_STATE_CHARS_BEFORE_CDATA;
			  return XML_STATE_CHARS;
      case XML_STATE_CHARS_BEFORE_CDATA:
			  fbgrow_reset(ctx->chars);
			}
		      xml_parse_cdata(ctx);
		      if (btell(ctx->chars))
		        {
			  fbgrow_rewind(ctx->chars);
			  return ctx->state = XML_STATE_CDATA;
			}
      case XML_STATE_CDATA:
		      fbgrow_reset(ctx->chars);
		    }
		}
	      else
		xml_fatal(ctx, "Unexpected character after '<!'");

	    else if (c != '/')
	      {
		/* STag | EmptyElemTag */
		xml_unget_char(ctx);
		if (btell(ctx->chars))
		  {
		    fbgrow_rewind(ctx->chars);
		    ctx->state = XML_STATE_CHARS_BEFORE_STAG;
		    return XML_STATE_CHARS;
      case XML_STATE_CHARS_BEFORE_STAG:
		    fbgrow_reset(ctx->chars);
		  }

		xml_parse_stag(ctx);
		if (ctx->want & XML_WANT_STAG)
		  return ctx->state = XML_STATE_STAG;
      case XML_STATE_STAG:
		if (ctx->flags & XML_FLAG_EMPTY_ELEM)
		  goto pop_element;
	      }

	    else
	      {
		/* ETag */
		if (btell(ctx->chars))
		  {
		    fbgrow_rewind(ctx->chars);
		    ctx->state = XML_STATE_CHARS_BEFORE_ETAG;
		    return XML_STATE_CHARS;
      case XML_STATE_CHARS_BEFORE_ETAG:
		    fbgrow_reset(ctx->chars);
		  }

		xml_parse_etag(ctx);
pop_element:
		if (ctx->want & XML_WANT_ETAG)
		  return ctx->state = XML_STATE_ETAG;
      case XML_STATE_ETAG:
		xml_pop_element(ctx);
		if (!ctx->elem)
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

#ifdef TEST

static void
error(struct xml_context *ctx)
{
  msg((ctx->err_code < XML_ERR_ERROR) ? L_WARN_R : L_ERROR_R, "XML %u: %s", xml_row(ctx), ctx->err_msg);
}

static void
test(struct fastbuf *in, struct fastbuf *out)
{
  struct xml_context ctx;
  xml_init(&ctx);
  ctx.h_warn = ctx.h_error = ctx.h_fatal = error;
  ctx.want = XML_WANT_ALL;
  ctx.flags |= XML_DOM_FREE;
  xml_set_source(&ctx, in);
  int state;
  while ((state = xml_next(&ctx)) >= 0)
    switch (state)
      {
	case XML_STATE_CHARS:
	  bprintf(out, "CHARS [%.*s]\n", (int)(ctx.chars->bstop - ctx.chars->buffer), ctx.chars->buffer);
	  break;
	case XML_STATE_STAG:
	  bprintf(out, "STAG <%s>\n", ctx.elem->name);
	  SLIST_FOR_EACH(struct xml_attr *, a, ctx.elem->attrs)
	    bprintf(out, "  ATTR %s=[%s]\n", a->name, a->val);
	  break;
	case XML_STATE_ETAG:
	  bprintf(out, "ETAG </%s>\n", ctx.elem->name);
	  break;
	case XML_STATE_COMMENT:
	  bprintf(out, "COMMENT [%.*s]\n", (int)(ctx.value->bstop - ctx.value->buffer), ctx.value->buffer);
	  break;
	case XML_STATE_PI:
	  bprintf(out, "PI [%s] [%.*s]\n", ctx.name, (int)(ctx.value->bstop - ctx.value->buffer), ctx.value->buffer);
	  break;
	case XML_STATE_CDATA:
	  bprintf(out, "CDATA [%.*s]\n", (int)(ctx.chars->bstop - ctx.chars->buffer), ctx.chars->buffer);
	  break;
	case XML_STATE_EOF:
	  bprintf(out, "EOF\n");
	  goto end;
	  break;
      }
end:
  xml_cleanup(&ctx);
}

int
main(void)
{
  struct fastbuf *in = bfdopen_shared(0, 1024);
  struct fastbuf *out = bfdopen_shared(1, 1024);
  test(in, out);
  bclose(out);
  return 0;
}

#endif
