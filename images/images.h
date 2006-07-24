#ifndef _IMAGES_IMAGES_H
#define _IMAGES_IMAGES_H

#include "lib/mempool.h"

/* image.c */

/* error handling */

enum image_error {
  IMAGE_ERR_OK = 0,
  IMAGE_ERR_UNSPECIFIED,
  IMAGE_ERR_NOT_IMPLEMENTED,
  IMAGE_ERR_INVALID_DIMENSIONS,
  IMAGE_ERR_INVALID_FILE_FORMAT,
  IMAGE_ERR_INVALID_PIXEL_FORMAT,
  IMAGE_ERR_READ_FAILED,
  IMAGE_ERR_WRITE_FAILED,
  IMAGE_ERR_MAX
};

struct image_thread {
  byte *err_msg;
  enum image_error err_code;
  struct mempool *pool;
};

void image_thread_init(struct image_thread *thread);
void image_thread_cleanup(struct image_thread *thread);

static inline void
image_thread_flush(struct image_thread *thread)
{
  thread->err_code = 0;
  thread->err_msg = NULL;
  mp_flush(thread->pool);
}

static inline void
image_thread_err(struct image_thread *thread, uns code, char *msg)
{
  thread->err_code = code;
  thread->err_msg = (byte *)msg;
}

void image_thread_err_format(struct image_thread *thread, uns code, char *msg, ...);

/* basic image manupulation */

enum color_space {
  COLOR_SPACE_UNKNOWN,
  COLOR_SPACE_GRAYSCALE,
  COLOR_SPACE_RGB,
  COLOR_SPACE_MAX
};

enum image_flag {
  IMAGE_COLOR_SPACE = 0x7,	/* mask for enum color_space */
  IMAGE_ALPHA = 0x8,		/* alpha channel */
  IMAGE_PIXELS_ALIGNED = 0x10,	/* align pixel size to the nearest power of two  */
  IMAGE_SSE_ALIGNED = 0x20,	/* align scanlines to multiples of 16 bytes (both start and size) */
  IMAGE_CHANNELS_FORMAT = IMAGE_COLOR_SPACE | IMAGE_ALPHA,
  IMAGE_PIXEL_FORMAT = IMAGE_CHANNELS_FORMAT | IMAGE_PIXELS_ALIGNED,
  IMAGE_ALIGNED = IMAGE_PIXELS_ALIGNED | IMAGE_SSE_ALIGNED,
};

struct image {
  byte *pixels;			/* left top pixel, there are at least sizeof(uns)
				   unsed bytes after the buffer (possible optimizations) */
  u32 cols;			/* number of columns */
  u32 rows;			/* number of rows */
  u32 pixel_size;		/* size of pixel (1, 2, 3 or 4) */
  u32 row_size;			/* scanline size in bytes */
  u32 image_size;		/* size of pixels buffer (rows * rows_size) */
  u32 flags;			/* enum image_flag */
};

struct image *image_new(struct image_thread *it, uns cols, uns rows, uns flags, struct mempool *pool);
struct image *image_clone(struct image_thread *it, struct image *src, uns flags, struct mempool *pool);
void image_destroy(struct image_thread *it, struct image *img); /* only with NULL mempool */
void image_clear(struct image_thread *it, struct image *img);

byte *color_space_to_name(enum color_space cs);
byte *image_channels_format_to_name(uns format);
uns image_name_to_channels_format(byte *name);

/* scale.c */

int image_scale(struct image_thread *thread, struct image *dest, struct image *src);
void image_dimensions_fit_to_box(u32 *cols, u32 *rows, u32 max_cols, u32 max_rows, uns upsample);

/* image-io.c */

enum image_format {
  IMAGE_FORMAT_UNKNOWN,
  IMAGE_FORMAT_JPEG,
  IMAGE_FORMAT_PNG,
  IMAGE_FORMAT_GIF,
  IMAGE_FORMAT_MAX
};

struct image_io {
  struct image *image;
  struct fastbuf *fastbuf;
  enum image_format format;
  struct mempool *pool;
  u32 cols;
  u32 rows;
  u32 flags;
  /* internals */
  struct image_thread *thread;
  struct mempool *internal_pool;
  int image_destroy;
  void *read_data;
  void (*read_cancel)(struct image_io *io);
  union {
    struct {
    } jpeg;
    struct {
    } png;
    struct {
    } gif;
  };
};

void image_io_init(struct image_thread *it, struct image_io *io);
void image_io_cleanup(struct image_io *io);
void image_io_reset(struct image_io *io);

int image_io_read_header(struct image_io *io);
struct image *image_io_read_data(struct image_io *io, int ref);
struct image *image_io_read(struct image_io *io, int ref);

int image_io_write(struct image_io *io);

byte *image_format_to_extension(enum image_format format);
enum image_format image_extension_to_format(byte *extension);
enum image_format image_file_name_to_format(byte *file_name);

/* internals */

#ifdef CONFIG_LIBJPEG
int libjpeg_read_header(struct image_io *io);
int libjpeg_read_data(struct image_io *io);
int libjpeg_write(struct image_io *io);
#endif

#ifdef CONFIG_LIBPNG
int libpng_read_header(struct image_io *io);
int libpng_read_data(struct image_io *io);
int libpng_write(struct image_io *io);
#endif

#ifdef CONFIG_LIBUNGIF
int libungif_read_header(struct image_io *io);
int libungif_read_data(struct image_io *io);
#endif

#ifdef CONFIG_LIBMAGICK
int libmagick_read_header(struct image_io *io);
int libmagick_read_data(struct image_io *io);
int libmagick_write(struct image_io *io);
#endif

#endif
