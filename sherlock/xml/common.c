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
#include "lib/stkstring.h"
#include "lib/ff-unicode.h"

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
  ctx->state = XML_STATE_EOF;
  va_end(args);
  if (ctx->h_fatal)
    ctx->h_fatal(ctx);
  xml_throw(ctx);
}

/*** Memory management ***/

void *
xml_hash_new(struct mempool *pool, uns size)
{
  void *tab = mp_alloc_zero(pool, size + XML_HASH_HDR_SIZE);
  *(void **)tab = pool;
  return tab + XML_HASH_HDR_SIZE;
}

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
	  n->type = XML_NODE_CHARS;
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

/*** Initialization ***/

static void
xml_do_init(struct xml_context *ctx)
{
  ctx->flags = XML_REPORT_ALL;
  xml_init_chars(ctx);
  xml_attrs_table_init(ctx);
}

void
xml_init(struct xml_context *ctx)
{
  bzero(ctx, sizeof(*ctx));
  ctx->pool = mp_new(65536);
  ctx->stack = mp_new(65536);
  xml_do_init(ctx);
  TRACE(ctx, "init");
}

void
xml_cleanup(struct xml_context *ctx)
{
  TRACE(ctx, "cleanup");
  xml_attrs_table_cleanup(ctx);
  xml_dtd_cleanup(ctx);
  xml_sources_cleanup(ctx);
  mp_delete(ctx->pool);
  mp_delete(ctx->stack);
}

void
xml_reset(struct xml_context *ctx)
{
  TRACE(ctx, "reset");
  struct mempool *pool = ctx->pool, *stack = ctx->stack;
  xml_attrs_table_cleanup(ctx);
  xml_dtd_cleanup(ctx);
  xml_sources_cleanup(ctx);
  mp_flush(pool);
  mp_flush(stack);
  bzero(ctx, sizeof(*ctx));
  ctx->pool = pool;
  ctx->stack = stack;
  xml_do_init(ctx);
}
