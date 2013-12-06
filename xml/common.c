/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <xml/xml.h>
#include <xml/dtd.h>
#include <xml/internals.h>
#include <ucw/stkstring.h>
#include <ucw/ff-unicode.h>

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

/*** Initialization ***/

static struct xml_context xml_defaults = {
  .flags = XML_SRC_EOF | XML_REPORT_ALL,
  .state = XML_STATE_START,
  .h_resolve_entity = xml_def_resolve_entity,
  .chars = {
    .name = "<xml_chars>",
    .spout = xml_spout_chars,
    .can_overwrite_buffer = 1,
  },
};

static void
xml_do_init(struct xml_context *ctx)
{
  xml_attrs_table_init(ctx);
}

void
xml_init(struct xml_context *ctx)
{
  *ctx = xml_defaults;
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
  *ctx = xml_defaults;
  ctx->pool = pool;
  ctx->stack = stack;
  xml_do_init(ctx);
}
