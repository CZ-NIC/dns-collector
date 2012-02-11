/*
 *	Image Library -- Image contexts
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/bbuf.h>
#include <images/images.h>
#include <images/error.h>

#include <string.h>

void
image_context_init(struct image_context *ctx)
{
  bzero(ctx, sizeof(*ctx));
  bb_init(&ctx->msg_buf);
  ctx->tracing_level = image_trace;
  ctx->msg_callback = image_context_msg_default;
}

void
image_context_cleanup(struct image_context *ctx)
{
  IMAGE_TRACE(ctx, 10, "Destroying image thread");
  bb_done(&ctx->msg_buf);
}

void
image_context_msg_default(struct image_context *ctx)
{
  msg(ctx->msg_code >> 24, "%s", ctx->msg);
}

void
image_context_msg_silent(struct image_context *ctx UNUSED)
{
}

void
image_context_msg(struct image_context *ctx, uns code, char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  image_context_vmsg(ctx, code, msg, args);
  va_end(args);
}

void
image_context_vmsg(struct image_context *ctx, uns code, char *msg, va_list args)
{
  ctx->msg_code = code;
  ctx->msg = bb_vprintf(&ctx->msg_buf, msg, args);
  ctx->msg_callback(ctx);
}
