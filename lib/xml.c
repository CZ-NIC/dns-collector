/*
 *	UCW Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/* TODO:
 * - various character encodings
 * - iface
 * - stack-like memory handling where possible
 */

#define LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mempool.h"
#include "lib/fastbuf.h"
#include "lib/ff-utf8.h"
#include "lib/chartype.h"
#include "lib/unicode.h"
#include "lib/xml.h"
#include "lib/hashfunc.h"
#include "lib/stkstring.h"
#include "charset/unicat.h"

#include <setjmp.h>

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

#include "obj/lib/xml-ucat.h"

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

/*** Reading of document/external entities ***/

static void NONRET
xml_eof(struct xml_context *ctx)
{
  ctx->err_msg = "Unexpected EOF";
  ctx->err_code = XML_ERR_EOF;
  xml_throw(ctx);
}

static void NONRET
xml_fatal_nested(struct xml_context *ctx)
{
  xml_fatal(ctx, "Entity is not tested correctly");
}

static inline void
xml_inc_depth(struct xml_context *ctx)
{
  ctx->depth++;
}

static inline void
xml_dec_depth(struct xml_context *ctx)
{
  if (unlikely(!ctx->depth))
    xml_fatal_nested(ctx);
  ctx->depth--;
}

static void
xml_push_source(struct xml_context *ctx, struct fastbuf *fb, uns flags)
{
  DBG("XML: xml_push_source");
  struct xml_source *osrc = ctx->sources;
  if (osrc)
    {
      osrc->bptr = ctx->bptr;
      osrc->bstop = ctx->bstop;
      osrc->depth = ctx->depth;
    }
  struct xml_source *src = mp_alloc(ctx->pool, sizeof(*src));
  src->next = osrc;
  src->flags = flags;
  src->fb = fb;
  ctx->depth = 0;
  ctx->sources = src;
  ctx->bstop = ctx->bptr = src->buf;
  if (flags & XML_SRC_SURROUND)
    {
      *ctx->bptr++ = 0x20;
      *ctx->bptr++ = xml_char_cat(0x20);
    }
}

void
xml_set_source(struct xml_context *ctx, struct fastbuf *fb)
{
  xml_push_source(ctx, fb, XML_SRC_DOCUMENT | XML_SRC_DECL);
}

static void
xml_pop_source(struct xml_context *ctx)
{
  DBG("XML: xml_pop_source");
  if (unlikely(ctx->depth))
    xml_fatal(ctx, "Invalid entity nesting");
  struct xml_source *src = ctx->sources;
  bclose(src->fb);
  ctx->sources = src = src->next;
  if (unlikely(!src))
    xml_eof(ctx);
  ctx->bptr = src->bptr;
  ctx->bstop = src->bstop;
  ctx->depth = src->depth;
}

static uns
xml_error_restricted(struct xml_context *ctx, uns c)
{
  xml_error(ctx, "Restricted char U+%04X", c);
  return UNI_REPLACEMENT;
}

static void xml_parse_decl(struct xml_context *ctx);

static void
xml_refill(struct xml_context *ctx)
{
  // FIXME:
  // -- various encodings, especially UTF-16
  // -- track col/row numbers
  // -- report incorrect encoding
  // -- deal with forbidden XML 1.1 newlines in xml/text decl
  do
    {
      struct xml_source *src = ctx->sources;
      uns c, t, t1, t2, f = src->flags;
      if (f & XML_SRC_EOF)
	xml_pop_source(ctx);
      else if (f & XML_SRC_DECL)
	xml_parse_decl(ctx);
      else
        {
          struct fastbuf *fb = src->fb;
	  if (ctx->bptr == ctx->bstop)
	    ctx->bptr = ctx->bstop = src->buf;
          u32 *bend = src->buf + ARRAY_SIZE(src->buf), *bstop = ctx->bstop, *last_0xd = (f & XML_SRC_NEW_LINE) ? bstop : bend;
          if (ctx->flags & XML_FLAG_VERSION_1_1)
            {
	      t2 = XML_CHAR_NEW_LINE_1_1;
	      t1 = XML_CHAR_UNRESTRICTED_1_1 & ~t2;
	    }
          else
            {
	      t2 = XML_CHAR_NEW_LINE_1_0;
	      t1 = XML_CHAR_VALID_1_0 & ~t2;
	    }
	  while (bstop < bend)
	    {
	      c = bget_utf8_32(fb);
	      t = xml_char_cat(c);
	      if (t & t1)
	        {
		  /* Typical branch */
		  *bstop++ = c;
		  *bstop++ = t;
		}
	      else if (t & t2)
	        {
		  /* New line
		   * XML 1.0: 0xA | 0xD | 0xD 0xA
		   * XML 1.1: 0xA | 0xD | 0xD 0xA | 0x85 | 0xD 0x85 | 0x2028 */
		  *bstop++ = 0xa;
		  *bstop++ = xml_char_cat(0xa);
		  if (c == 0xd)
		    last_0xd = bstop;
		  else if (c != 0x2028 && last_0xd != bstop - 2)
		    bstop -= 2;
		}
	      else if ((int)c >= 0)
	        {
		  /* Restricted character */
		  c = xml_error_restricted(ctx, c);
		  *bstop++ = c;
		  *bstop++ = xml_char_cat(c);
		}
	      else
	        {
		  /* EOF */
		  if (f & XML_SRC_SURROUND)
		    {
		      *bstop++ = 0x20;
		      *bstop++ = xml_char_cat(0x20);
		    }
		  f |= XML_SRC_EOF;
		  break;
		}
	    }
	  if (last_0xd == bstop)
	    f |= XML_SRC_NEW_LINE;
	  else
	    f &= ~XML_SRC_NEW_LINE;
	  ctx->sources->flags = f;
	  ctx->bstop = bstop;
	  DBG("XML: refilled %u characters", (uns)(ctx->bstop - ctx->bptr) / 2);
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
  if (unlikely(!(xml_peek_cat(ctx) & XML_CHAR_ENC_SNAME)))
    xml_fatal(ctx, "Invalid character in the encoding name");
  while(1)
    {
      p = mp_spread(ctx->pool, p, 2);
      *p++ = xml_skip_char(ctx);
      if (xml_get_char(ctx) == q)
	break;
      if (unlikely(!(xml_last_cat(ctx) & XML_CHAR_ENC_NAME)))
	xml_fatal(ctx, "Invalid character in the encoding name");
    }
  *p++ = 0;
  return mp_end(ctx->pool, p);
}

/* Document/external entity header */

static void
xml_detect_encoding(struct xml_context *ctx)
{
  DBG("XML: xml_detect_encoding");
  struct xml_source *src = ctx->sources;
  struct fastbuf *fb = src->fb;
  char *detected_encoding = NULL;
  uns x = 0, l = 0, c, z = 1;
  while (l < 4)
    {
      if (!~(c = bgetc(fb)))
        {
	  src->flags |= XML_SRC_EOF;
          break;
	}
      else if (!c || c >= 0xfe || c == 0xa7 || c == 0x94)
	z = 0;
      else if ((c < 0x3c || c > 0x78))
        {
	  bungetc(fb);
	  break;
	}
      x = (x << 8) + c;
      l++;
    }
  if (z)
    z = x;
  else if (l == 2)
    switch (x)
      {
	case 0xFEFF:
	  xml_fatal(ctx, "UTF-16BE encoding not supported");
	case 0xFFFE:
	  xml_fatal(ctx, "UTF-16LE encoding not supported");
	default:
	  goto cannot_detect;
      }
  else if (l == 4)
    switch (x)
      {
	case 0x0000FEFF:
	  xml_fatal(ctx, "UCS-4BE encoding not supported");
	case 0xFFFE0000:
	  xml_fatal(ctx, "UCS-4LE encoding not supported");
	case 0x0000FFFE:
	  xml_fatal(ctx, "UCS-4 encoding (order 2143) not supported");
	case 0xFEFF0000:
	  xml_fatal(ctx, "UCS-4 encoding (order 3412) not supported");
	case 0x0000003c:
	  xml_fatal(ctx, "UCS-4BE encoding not supported");
	case 0x3c000000:
	  xml_fatal(ctx, "UCS-4LE encoding not supported");
	case 0x00003c00:
	  xml_fatal(ctx, "UCS-4 encoding (order 2143) not supported");
	case 0x003c0000:
	  xml_fatal(ctx, "UCS-4 encoding (order 3412) not supported");
	case 0x003c003F:
	  xml_fatal(ctx, "UTF-16BE encoding not supported");
	case 0x3C003F00:
	  xml_fatal(ctx, "UTF-16LE encoding not supported");
	case 0x3C3F786D:
	  xml_fatal(ctx, "EBCDIC encoding not supported");
	default:
	  goto cannot_detect;
      }
  else
cannot_detect:
    xml_fatal(ctx, "Cannot detect the encoding");
  ctx->bptr = ctx->bstop = src->buf + 8;
  while (z)
    {
      c = z & 0xff;
      z >>= 8;
      *--ctx->bptr = xml_char_cat(c);
      *--ctx->bptr = c;
    }
  if (!detected_encoding && ctx->bstop == ctx->bptr && xml_peek_char(ctx) == 0xfeff)
    xml_skip_char(ctx);
  DBG("XML: Detected encoding: %s", detected_encoding ? : "UTF-8");
  if (!(src->flags & XML_SRC_EOF))
    xml_refill(ctx);
}

static void
xml_parse_decl(struct xml_context *ctx)
{
  DBG("XML: xml_parse_decl");
  ctx->sources->flags &= ~XML_SRC_DECL;
  xml_detect_encoding(ctx);
  uns document = ctx->sources->flags & XML_SRC_DOCUMENT;
  u32 *bptr = ctx->bptr;
  uns have_decl =
    (12 <= ctx->bstop - ctx->bptr &&
    bptr[0] == '<' && bptr[2] == '?' && (bptr[4] & 0xdf) == 'X' && (bptr[6] & 0xdf) == 'M' && (bptr[8] & 0xdf) == 'L' &&
    (bptr[11] & XML_CHAR_WHITE));
  if (!have_decl)
    {
      if (document)
        xml_fatal(ctx, "Missing or corrupted XML declaration header");
      return;
    }
  ctx->bptr += 12;

  /* FIXME: the header must not contain exotic newlines */
  xml_parse_white(ctx, 0);

  if (xml_peek_char(ctx) == 'v')
    {
      xml_parse_seq(ctx, "version");
      xml_parse_eq(ctx);
      char *version = xml_parse_pubid_literal(ctx);
      DBG("XML: Version=%s", version);
      if (document)
        {
	  ctx->version_str = version;
	  if (!strcmp(ctx->version_str, "1.0"))
	    ;
	  else if (!strcmp(ctx->version_str, "1.1"))
	    ctx->flags |= XML_FLAG_VERSION_1_1;
	  else
	    xml_fatal(ctx, "Unsupported XML version");
	}
      else if (strcmp(version, ctx->version_str))
	xml_error(ctx, "Mixed XML versions");
    }
  else if (document)
    xml_fatal(ctx, "Missing XML version");

  // FIXME: TextDecl must contain encoding
  if (!xml_parse_white(ctx, 0))
    goto end;
  if (xml_peek_char(ctx) == 'e')
    {
      xml_parse_seq(ctx, "encoding");
      xml_parse_eq(ctx);
      ctx->encoding = xml_parse_encoding_name(ctx);
      DBG("encoding=%s", ctx->encoding);
      // FIXME: check encoding
      if (!xml_parse_white(ctx, 0))
	goto end;
    }

  if (document && xml_peek_char(ctx) == 's')
    {
      xml_parse_seq(ctx, "standalone");
      xml_parse_eq(ctx);
      uns c = xml_parse_quote(ctx);
      if (ctx->standalone = (xml_peek_char(ctx) == 'y'))
	xml_parse_seq(ctx, "yes");
      else
        xml_parse_seq(ctx, "no");
      xml_parse_char(ctx, c);
      DBG("standalone=%d", ctx->standalone);
      xml_parse_white(ctx, 0);
    }
end:
  xml_parse_seq(ctx, "?>");
}

/*** Document Type Definition (DTD) ***/

/* Notations */

#define HASH_PREFIX(x) xml_dtd_notns_##x
#define HASH_NODE struct xml_dtd_notn
#define HASH_KEY_STRING name
#define HASH_AUTO_POOL 1024
#define HASH_ZERO_FILL
#define HASH_TABLE_DYNAMIC
#define HASH_WANT_FIND
#define HASH_WANT_LOOKUP
#define HASH_WANT_CLEANUP
#include "lib/hashtable.h"

/* General entities */

#define HASH_PREFIX(x) xml_dtd_ents_##x
#define HASH_NODE struct xml_dtd_ent
#define HASH_KEY_STRING name
#define HASH_AUTO_POOL 1024
#define HASH_ZERO_FILL
#define HASH_TABLE_DYNAMIC
#define HASH_WANT_FIND
#define HASH_WANT_LOOKUP
#define HASH_WANT_CLEANUP
#include "lib/hashtable.h"

static void
xml_dtd_declare_trivial_gent(struct xml_context *ctx, char *name, char *text)
{
  struct xml_dtd *dtd = ctx->dtd;
  struct xml_dtd_ent *ent = xml_dtd_ents_lookup(dtd->tab_gents, name);
  if (ent->flags & XML_DTD_ENT_DECLARED)
    {
      xml_warn(ctx, "Entity &%s; already declared", name);
      return;
    }
  slist_add_tail(&dtd->gents, &ent->n);
  ent->flags = XML_DTD_ENT_DECLARED | XML_DTD_ENT_TRIVIAL;
  ent->text = text;
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
      return (ent->flags & XML_DTD_ENT_DECLARED) ? ent : NULL;
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
  return (ent->flags & XML_DTD_ENT_DECLARED) ? ent : NULL;
}

/* Elements */

#define HASH_PREFIX(x) xml_dtd_elems_##x
#define HASH_NODE struct xml_dtd_elem
#define HASH_KEY_STRING name
#define HASH_TABLE_DYNAMIC
#define HASH_AUTO_POOL 1024
#define HASH_ZERO_FILL
#define HASH_WANT_LOOKUP
#define HASH_WANT_CLEANUP
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
#define HASH_AUTO_POOL 1024
#define HASH_ZERO_FILL
#define HASH_TABLE_DYNAMIC
#define HASH_KEY_COMPLEX(x) x elem, x name
#define HASH_KEY_DECL struct xml_dtd_elem *elem, char *name
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_WANT_CLEANUP
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
#define HASH_AUTO_POOL 1024
#define HASH_TABLE_DYNAMIC
#define HASH_KEY_COMPLEX(x) x attr, x val
#define HASH_KEY_DECL struct xml_dtd_attr *attr, char *val
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_WANT_CLEANUP
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
#define HASH_AUTO_POOL 1024
#define HASH_TABLE_DYNAMIC
#define HASH_KEY_COMPLEX(x) x attr, x notn
#define HASH_KEY_DECL struct xml_dtd_attr *attr, struct xml_dtd_notn *notn
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_WANT_CLEANUP
#include "lib/hashtable.h"

/* DTD initialization/cleanup */

static void
xml_dtd_init(struct xml_context *ctx)
{
  ctx->dtd = mp_alloc_zero(ctx->pool, sizeof(*ctx->dtd));
  xml_dtd_ents_init(ctx->dtd->tab_gents = mp_alloc_zero(ctx->pool, sizeof(struct xml_dtd_ents_table)));
  xml_dtd_ents_init(ctx->dtd->tab_pents = mp_alloc_zero(ctx->pool, sizeof(struct xml_dtd_ents_table)));
  xml_dtd_notns_init(ctx->dtd->tab_notns = mp_alloc_zero(ctx->pool, sizeof(struct xml_dtd_notns_table)));
  xml_dtd_elems_init(ctx->dtd->tab_elems = mp_alloc_zero(ctx->pool, sizeof(struct xml_dtd_elems_table)));
  xml_dtd_attrs_init(ctx->dtd->tab_attrs = mp_alloc_zero(ctx->pool, sizeof(struct xml_dtd_attrs_table)));
  xml_dtd_evals_init(ctx->dtd->tab_evals = mp_alloc_zero(ctx->pool, sizeof(struct xml_dtd_evals_table)));
  xml_dtd_enotns_init(ctx->dtd->tab_enotns = mp_alloc_zero(ctx->pool, sizeof(struct xml_dtd_enotns_table)));
  xml_dtd_declare_default_gents(ctx);
}

static void
xml_dtd_cleanup(struct xml_context *ctx)
{
  if (!ctx->dtd)
    return;
  xml_dtd_ents_cleanup(ctx->dtd->tab_gents);
  xml_dtd_ents_cleanup(ctx->dtd->tab_pents);
  xml_dtd_notns_cleanup(ctx->dtd->tab_notns);
  xml_dtd_elems_cleanup(ctx->dtd->tab_elems);
  xml_dtd_attrs_cleanup(ctx->dtd->tab_attrs);
  xml_dtd_evals_cleanup(ctx->dtd->tab_evals);
  xml_dtd_enotns_cleanup(ctx->dtd->tab_enotns);
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
	  return;
	}
    }
  while (1)
    if (xml_get_char(ctx) == '?')
      if (xml_get_char(ctx) == '>')
	break;
      else
	xml_unget_char(ctx);
}

/* Character references */

static uns
xml_parse_char_ref(struct xml_context *ctx)
{
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
    return v;
  xml_error(ctx, "Expected ';'");
recover:
  while (xml_last_char(ctx) != ';')
    xml_get_char(ctx);
  return UNI_REPLACEMENT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
xml_parse_parameter_ref(struct xml_context *ctx)
{
  char *name = xml_parse_name(ctx);
  xml_parse_char(ctx, ';');
  struct xml_dtd_ent *ent = xml_dtd_ents_find(ctx->dtd->tab_pents, name);
  if (!ent || !(ent->flags & XML_DTD_ENT_DECLARED))
    {
      xml_error(ctx, "Reference to unknown parameter entity %%%s", name);
      return;
    }
  if (ent->flags & XML_DTD_ENT_VISITED)
    {
      xml_error(ctx, "Cycled references to parameter entity %%%s", name);
      return;
    }
  if (ent->flags & XML_DTD_ENT_EXTERNAL)
    {
      // FIXME:
      xml_error(ctx, "Support for external parsed entities not implemented");
      return;
    }
  ent->flags |= XML_DTD_ENT_VISITED; // FIXME: clear
  struct fastbuf *fb = mp_alloc(ctx->pool, sizeof(*fb));
  fbbuf_init_read(fb, ent->text, ent->len, 0);
  xml_push_source(ctx, fb, 0);
}

static inline void
xml_check_parameter_ref(struct xml_context *ctx)
{
  if (xml_get_char(ctx) != '%')
    {
      xml_unget_char(ctx);
      return;
    }
  xml_parse_parameter_ref(ctx);
}

static void
xml_parse_external_id(struct xml_context *ctx, struct xml_ext_id *eid, uns allow_public)
{
  bzero(eid, sizeof(*eid));
  uns c = xml_get_char(ctx);
  if (c == 'S')
    {
      xml_parse_seq(ctx, "YSTEM");
      xml_parse_white(ctx, 1);
      eid->system_id = xml_parse_system_literal(ctx);
    }
  else if (c == 'P')
    {
      xml_parse_seq(ctx, "UBLIC");
      xml_parse_white(ctx, 1);
      eid->public_id = xml_parse_pubid_literal(ctx);
      if (xml_parse_white(ctx, 1))
	if ((c = xml_get_char(ctx)) == '\'' || c == '"' || !allow_public)
	  {
	    xml_unget_char(ctx);
            eid->system_id = xml_parse_system_literal(ctx);
	  }
        else
	  xml_unget_char(ctx);
    }
  else
    xml_fatal(ctx, "Expected an external ID");
}

static void
xml_parse_notation_decl(struct xml_context *ctx)
{
  /* NotationDecl ::= '<!NOTATION' S Name S (ExternalID | PublicID) S? '>'*/
  xml_parse_white(ctx, 1);
  struct xml_dtd_notn *notn = xml_dtd_notns_lookup(ctx->dtd->tab_notns, xml_parse_name(ctx));
  xml_parse_white(ctx, 1);
  struct xml_ext_id eid;
  xml_parse_external_id(ctx, &eid, 1);
  xml_parse_white(ctx, 0);
  xml_parse_char(ctx, '>');
  if (notn->flags & XML_DTD_NOTN_DECLARED)
    xml_warn(ctx, "Notation %s already declared", notn->name);
  else
    {
      notn->flags = XML_DTD_NOTN_DECLARED;
      notn->eid = eid;
    }
}

static void
xml_parse_internal_subset(struct xml_context *ctx)
{
  while (1)
    {
      xml_parse_white(ctx, 0);
      uns c = xml_get_char(ctx);
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
		    //xml_parse_entity_decl(ctx);
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
	xml_parse_parameter_ref(ctx);
      else if (c == ']')
	break;
      else
	goto invalid_markup;
    }
  return;
invalid_markup:
  xml_fatal(ctx, "Invalid markup in the internal subset");
}

/*----------------------------------------------*/


/* FIXME */

struct xml_attribute_table;

#define HASH_PREFIX(x) xml_attribute_##x
#define HASH_NODE struct xml_attribute
#define HASH_KEY_COMPLEX(x) x element, x name
#define HASH_KEY_DECL struct xml_element *element, char *name
#define HASH_TABLE_DYNAMIC
#define HASH_AUTO_POOL 1024

#define HASH_GIVE_HASHFN

static inline uns
xml_attribute_hash(struct xml_attribute_table *t UNUSED, struct xml_element *e, char *n)
{
  return hash_pointer(e) ^ hash_string(n);
}

#define HASH_GIVE_EQ

static inline int
xml_attribute_eq(struct xml_attribute_table *t UNUSED, struct xml_element *e1, char *n1, struct xml_element *e2, char *n2)
{
  return (e1 == e2) && !strcmp(n1, n2);
}

#define HASH_GIVE_INIT_KEY

static inline void
xml_attribute_init_key(struct xml_attribute_table *t UNUSED, struct xml_attribute *a, struct xml_element *e, char *name)
{
  a->element = e;
  a->name = name;
  a->value = NULL;
  a->next = e->attrs;
  e->attrs = a;
}

#define HASH_WANT_CLEANUP
#define HASH_WANT_REMOVE
#define HASH_WANT_LOOKUP
#define HASH_WANT_FIND
#include "lib/hashtable.h"


/*
#define HASH_PREFIX(x) xml_parsed_entities_##x
#define HASH_NODE struct xml_parsed_entity
#define HASH_KEY_STRING name
#define HASH_TABLE_DYNAMIC
#define HASH_AUTO_POOL 1024
#define HASH_WANT_CLEANUP
#include "lib/hashtable.h"
*/

void
xml_init(struct xml_context *ctx)
{
  bzero(ctx, sizeof(*ctx));
  ctx->pool = mp_new(65536);
  ctx->chars = fbgrow_create(4096);
  ctx->value = fbgrow_create(4096);
  xml_dtd_init(ctx);
}

void
xml_cleanup(struct xml_context *ctx)
{
  xml_dtd_cleanup(ctx);
  bclose(ctx->value);
  bclose(ctx->chars);
  mp_delete(ctx->pool);
}

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
xml_parse_ref_entity(struct xml_context *ctx UNUSED, struct fastbuf *out UNUSED, struct xml_dtd_ent *entity UNUSED)
{
#if 0
  for (struct xml_dtd_ent_node *node = entity->list; node; node = node->next)
    if (node->len)
      bwrite(out, node->ptr, node->len);
    else
      xml_parse_ref_entity(ctx, out, node->ptr); // FIXME: do not call the recursion on stack -- could cause segfault
#endif
}

static void
xml_parse_ref(struct xml_context *ctx, struct fastbuf *out)
{
  if (xml_get_char(ctx) == '#')
    {
      uns c = xml_parse_char_ref(ctx);
      bput_utf8_32(out, c);
    }
  else
    {
#if 0
      xml_unget_char(ctx);
      mp_push(ctx->pool);
      char *name = xml_parse_name(ctx);
      struct xml_parsed_entity *entity = xml_find_parsed_entity(ctx, name);
      mp_pop(ctx->pool);
      xml_parse_char(ctx, ';');
      xml_parse_ref_entity(ctx, out, entity);
#endif
    }
}

static void
xml_parse_chars(struct xml_context *ctx)
{
  DBG("parse_chars");
  struct fastbuf *out = ctx->chars;
  uns c;
  while ((c = xml_get_char(ctx)) != '<')
    if (c == '&')
      xml_parse_ref(ctx, out);
    else
      bput_utf8_32(out, c);
  xml_unget_char(ctx);
}

static void
xml_parse_attr(struct xml_context *ctx)
{
  DBG("parse_attr");
  struct xml_element *e = ctx->element;
  char *name = xml_parse_name(ctx);
  struct xml_attribute *a = xml_attribute_lookup(ctx->attribute_table, e, name);
  if (a->value)
    xml_fatal(ctx, "Attribute is not unique");
  xml_parse_eq(ctx);
  // FIXME
  char *value = xml_parse_system_literal(ctx);
  a->value = value;
}

static uns
xml_parse_stag(struct xml_context *ctx)
{
  DBG("parse_stag");
  mp_push(ctx->pool);
  struct xml_element *e = mp_alloc_zero(ctx->pool, sizeof(*e));
  e->parent = ctx->element;
  ctx->element = e;
  e->name = xml_parse_name(ctx);
  while (1)
    {
      uns white = xml_parse_white(ctx, 0);
      uns c = xml_get_char(ctx);
      if (c == '/')
        {
	  xml_parse_char(ctx, '>');
	  return 1;
	}
      else if (c == '>')
	return 0;
      else if (!white)
	xml_fatal(ctx, "Expected a white space");
      xml_unget_char(ctx);
      xml_parse_attr(ctx);
    }
}

static void
xml_parse_etag(struct xml_context *ctx)
{
  DBG("parse_etag");
  struct xml_element *e = ctx->element;
  ASSERT(e);
  char *name = xml_parse_name(ctx);
  if (strcmp(name, e->name))
    xml_fatal(ctx, "Invalid ETag, expected '%s'", e->name);
  xml_parse_white(ctx, 0);
  xml_parse_char(ctx, '>');
  // FIXME: remove on pooled hashtable?
  for (struct xml_attribute *a = e->attrs; a; a = a->next)
    xml_attribute_remove(ctx->attribute_table, a);
  ctx->element = e->parent;
  mp_pop(ctx->pool);
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
xml_parse_entity_decl(struct xml_context *ctx)
{
  struct xml_dtd *dtd = ctx->dtd;
  xml_parse_white(ctx, 1);

  uns flags = (xml_get_char(ctx) == '%') ? XML_DTD_ENT_PARAMETER : 0;
  if (flags)
    xml_parse_white(ctx, 1);
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

  uns sep = xml_get_char(ctx), c;
  if (sep == '\'' || sep == '"')
    {
      /* Internal entity:
       * EntityValue ::= '"' ([^%&"] | PEReference | Reference)* '"' | "'" ([^%&'] | PEReference | Reference)* "'" */
      struct fastbuf *out = ctx->value;
      uns sep = c;
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
      xml_parse_external_id(ctx, &eid, 0);
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
}

static void
xml_parse_doctype_decl(struct xml_context *ctx)
{
  if (ctx->document_type)
    xml_fatal(ctx, "Multiple document types not allowed");
  xml_parse_seq(ctx, "DOCTYPE");
  xml_parse_white(ctx, 1);
  ctx->document_type = xml_parse_name(ctx);
  DBG("XML: DocumentType=%s", ctx->document_type);
  uns white = xml_parse_white(ctx, 0);
  uns c = xml_peek_char(ctx);
  if (c != '>' && c != '[' && white)
    {
      xml_parse_external_id(ctx, &ctx->eid, 0);
      xml_parse_white(ctx, 0);
    }
  if (ctx->h_doctype_decl)
    ctx->h_doctype_decl(ctx);
}

int
xml_next(struct xml_context *ctx)
{
  /* A nasty state machine */

  DBG("XML: xml_next (state=%u)", ctx->state);
  jmp_buf throw_buf;
  ctx->throw_buf = &throw_buf;
  if (setjmp(throw_buf))
    {
error:
      if (ctx->err_code == XML_ERR_EOF && ctx->h_fatal)
	ctx->h_fatal(ctx);
      ctx->state = XML_STATE_FATAL;
      DBG("XML: raised fatal error");
      return -1;
    }
  uns c;
  switch (ctx->state)
    {
      case XML_STATE_FATAL:
	return -1;

      case XML_STATE_START:
	DBG("XML: Entering Prolog");
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
		    // FIXME
		    while (xml_get_char(ctx) != ']');
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
	    if (xml_get_char(ctx) != '<')
	      {
		/* CharData */
	        xml_unget_char(ctx);
	        xml_parse_chars(ctx);
		continue;
	      }
first_tag: ;

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

		if (xml_parse_stag(ctx))
		  {
		  }
		if (ctx->want & XML_WANT_STAG)
		  return ctx->state = XML_STATE_STAG;
      case XML_STATE_STAG:
		// FIXME: EmptyElemTag
		;

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

		if (ctx->want & XML_WANT_ETAG)
		  return ctx->state = XML_STATE_ETAG;
      case XML_STATE_ETAG:

		xml_parse_etag(ctx);

		if (!ctx->element)
		  goto epilog;
	      }
	  }

epilog:
	/* Misc* */
        DBG("XML: Entering epilog");
	while (1)
	  {
	    /* Epilog whitespace is the only place, where a valid document can reach EOF */
	    if (setjmp(throw_buf))
	      if (ctx->err_code == XML_ERR_EOF)
	        {
		  DBG("XML: Reached EOF");
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
  msg((ctx->err_code < XML_ERR_ERROR) ? L_WARN_R : L_ERROR_R, "XML: %s", ctx->err_msg);
}

static void
test(struct fastbuf *in, struct fastbuf *out)
{
  struct xml_context ctx;
  xml_init(&ctx);
  ctx.h_warn = ctx.h_error = ctx.h_fatal = error;
  ctx.want = XML_WANT_ALL;
  xml_set_source(&ctx, in);
  int state;
  while ((state = xml_next(&ctx)) >= 0)
    switch (state)
      {
	case XML_STATE_CHARS:
	  bprintf(out, "CHARS [%.*s]\n", (int)(ctx.chars->bstop - ctx.chars->buffer), ctx.chars->buffer);
	  break;
	case XML_STATE_STAG:
	  bprintf(out, "STAG <%s>\n", ctx.element->name);
	  for (struct xml_attribute *a = ctx.element->attrs; a; a = a->next)
	    bprintf(out, "  ATTR %s=[%s]\n", a->name, a->value);
	  break;
	case XML_STATE_ETAG:
	  bprintf(out, "ETAG </%s>\n", ctx.element->name);
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
	default:
          bprintf(out, "STATE %u\n", state);
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
