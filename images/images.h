#ifndef _IMAGES_IMAGES_H
#define _IMAGES_IMAGES_H

enum image_flag {
  IMAGE_GRAYSCALE = 0x1,	/* grayscale image */
};

struct image_data {
  uns flags;			/* enum image_flag */
  uns width;			/* number of columns */
  uns height;			/* number of rows */
  uns size;			/* buffer size in bytes */
  byte *pixels;			/* RGB */
};

#if 0

enum image_format {
  IMAGE_FORMAT_UNDEFINED = 0,
  IMAGE_FORMAT_JPEG,
  IMAGE_FORMAT_PNG,
  IMAGE_FORMAT_GIF
};

struct image_io {
  struct mempool *pool;
  struct fastbuf *fb;
  enum image_format format;
  struct image_data image;
  void *internals;
  union {
    struct {
    } jpeg;
    struct {
    } png;
    struct {
    } gif;
  };
};

void image_open(struct image_io *io, struct fastbuf *fb, struct mempool *pool);
void image_close(struct image_io *io);
int image_read_header(struct image_io *io);
int image_read_data(struct image_io *io);
int image_read(struct image_io *io);
int image_write(struct image_io *io);

#endif

#endif

