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

static inline void
image_thread_err_dup(struct image_thread *thread, uns code, char *msg)
{
  thread->err_code = code;
  thread->err_msg = mp_strdup(thread->pool, msg);
}

void image_thread_err_format(struct image_thread *thread, uns code, char *msg, ...);

/* basic image manupulation */

#define IMAGE_MAX_SIZE		0xffffU	/* maximum number of cols/rows, must be <(1<<16) */
#define IMAGE_SSE_ALIGN_SIZE	(MAX(16, sizeof(uns)))

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
  IMAGE_NEED_DESTROY = 0x40,	/* image is allocated with xmalloc */
  IMAGE_GAPS_PROTECTED = 0x80,	/* cannot access gaps between rows */
  IMAGE_CHANNELS_FORMAT = IMAGE_COLOR_SPACE | IMAGE_ALPHA,
  IMAGE_PIXEL_FORMAT = IMAGE_CHANNELS_FORMAT | IMAGE_PIXELS_ALIGNED,
  IMAGE_ALIGNED = IMAGE_PIXELS_ALIGNED | IMAGE_SSE_ALIGNED,
  IMAGE_NEW_FLAGS = IMAGE_PIXEL_FORMAT | IMAGE_SSE_ALIGNED,
  IMAGE_INTERNAL_FLAGS = IMAGE_NEED_DESTROY | IMAGE_GAPS_PROTECTED,
};

struct image {
  byte *pixels;			/* left top pixel, there are at least sizeof(uns)
				   unsed bytes after the buffer (possible optimizations) */
  u32 cols;			/* number of columns */
  u32 rows;			/* number of rows */
  u32 pixel_size;		/* size of pixel (1, 2, 3 or 4) */
  u32 row_size;			/* scanline size in bytes */
  u32 image_size;		/* rows * row_size */
  u32 flags;			/* enum image_flag */
};

struct image *image_new(struct image_thread *it, uns cols, uns rows, uns flags, struct mempool *pool);
struct image *image_clone(struct image_thread *it, struct image *src, uns flags, struct mempool *pool);
void image_destroy(struct image *img);
void image_clear(struct image_thread *it, struct image *img);
struct image *image_init_matrix(struct image_thread *it, struct image *img, byte *pixels, uns cols, uns rows, uns row_size, uns flags);
struct image *image_init_subimage(struct image_thread *it, struct image *img, struct image *src, uns left, uns top, uns cols, uns rows);

static inline int
image_dimensions_valid(uns cols, uns rows)
{
  return cols && rows && cols <= IMAGE_MAX_SIZE && rows <= IMAGE_MAX_SIZE;
}

byte *color_space_to_name(enum color_space cs);
byte *image_channels_format_to_name(uns format);
uns image_name_to_channels_format(byte *name);

struct color {
  byte c[3];
  byte color_space;
} PACKED;

/* scale.c */

int image_scale(struct image_thread *thread, struct image *dest, struct image *src);
void image_dimensions_fit_to_box(u32 *cols, u32 *rows, u32 max_cols, u32 max_rows, uns upsample);

/* alpha.c */

int image_apply_background(struct image_thread *thread, struct image *dest, struct image *src, struct color *background);

/* image-io.c */

enum image_format {
  IMAGE_FORMAT_UNDEFINED,
  IMAGE_FORMAT_JPEG,
  IMAGE_FORMAT_PNG,
  IMAGE_FORMAT_GIF,
  IMAGE_FORMAT_MAX
};

struct image_io {
  					/*  R - read_header input */
  					/*   H - read_header output */
  					/*    I - read_data input */
  					/*     O - read_data output */
  					/*      W - write input */

  struct image *image;			/* [   OW] - image data */
  enum image_format format;		/* [R   W] - file format (IMAGE_FORMAT_x) */
  struct fastbuf *fastbuf;      	/* [R   W] - source/destination stream */
  struct mempool *pool;			/* [  I  ] - parameter to image_new */
  u32 cols;				/* [ HI  ] - number of columns, parameter to image_new */
  u32 rows;				/* [ HI  ] - number of rows, parameter to image_new */
  u32 flags;				/* [ HI  ] - see enum image_io_flags */
  u32 jpeg_quality;			/* [    W] - JPEG compression quality (1..100) */
  u32 number_of_colors;			/* [ H   ] - number of image colors */
  struct color background_color;	/* [ HI  ] - background color, zero if undefined */

  /* internals */
  struct image_thread *thread;
  struct mempool *internal_pool;
  void *read_data;
  void (*read_cancel)(struct image_io *io);
};

enum image_io_flags {
  IMAGE_IO_IMAGE_FLAGS = 0xffff,	/* [ HI  ] - mask of parameters to image new, read_header fills IMAGE_CHANNELS_FORMAT */
  IMAGE_IO_NEED_DESTROY = 0x10000,	/* [   O ] - enables automatic call of image_destroy */
  IMAGE_IO_HAS_PALETTE = 0x20000,	/* [ H   ] - true for image with indexed colors */
  IMAGE_IO_USE_BACKGROUND = 0x40000,	/* [  I  ] - merge transparent pixels with background_color */
};

int image_io_init(struct image_thread *it, struct image_io *io);
void image_io_cleanup(struct image_io *io);
void image_io_reset(struct image_io *io);

int image_io_read_header(struct image_io *io);
struct image *image_io_read_data(struct image_io *io, int ref);
struct image *image_io_read(struct image_io *io, int ref);

int image_io_write(struct image_io *io);

byte *image_format_to_extension(enum image_format format);
enum image_format image_extension_to_format(byte *extension);
enum image_format image_file_name_to_format(byte *file_name);

#endif
