#ifndef _IMAGES_ERROR_H
#define _IMAGES_ERROR_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define image_trace ucw_image_trace
#endif

extern uns image_trace; /* ImageLib.Trace */ 

/* Error codes */

enum image_msg_code {
  IMAGE_MSG_TYPE = 0xff000000,
  IMAGE_MSG_TRACE = (L_DEBUG << 24),
  IMAGE_MSG_WARN = (L_WARN << 24),
  IMAGE_MSG_ERROR = (L_ERROR << 24),
  IMAGE_TRACE_LEVEL = 0x0000ffff,
  IMAGE_WARN_TYPE = 0x0000ffff,
  IMAGE_WARN_SUBTYPE = 0x00ff0000,
  IMAGE_ERROR_TYPE = 0x0000ffff,
  IMAGE_ERROR_SUBTYPE = 0x00ff0000,
  IMAGE_ERROR_NOT_IMPLEMENTED = 1,
  IMAGE_ERROR_INVALID_DIMENSIONS = 2,
  IMAGE_ERROR_INVALID_FILE_FORMAT = 3,
  IMAGE_ERROR_INVALID_PIXEL_FORMAT = 4,
  IMAGE_ERROR_READ_FAILED = 5,
  IMAGE_ERROR_WRITE_FAILED = 6,
};

/* Useful macros */

#define IMAGE_WARN(ctx, type, msg...) image_context_msg((ctx), IMAGE_MSG_WARN | (type), msg)
#define IMAGE_ERROR(ctx, type, msg...) image_context_msg((ctx), IMAGE_MSG_ERROR | (type), msg)

#define IMAGE_TRACE(ctx, level, msg...) do { \
	struct image_context *_ctx = (ctx); uns _level = (level); \
	if (_level < _ctx->tracing_level) image_context_msg(_ctx, IMAGE_MSG_TRACE | _level, msg); } while (0)

#endif
