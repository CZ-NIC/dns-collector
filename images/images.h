#ifndef _IMAGES_IMAGES_H
#define _IMAGES_IMAGES_H

enum image_flag {
  IMAGE_GRAYSCALE = 0x1,	/* grayscale image */
};

struct image {
  uns flags;			/* enum image_flag */
  uns width;			/* number of columns */
  uns height;			/* number of rows */
  uns size;			/* buffer size in bytes */
  byte *pixels;			/* RGB */
};

enum image_format {
  IMAGE_FORMAT_UNDEFINED = 0,
  IMAGE_FORMAT_JPEG,
  IMAGE_FORMAT_PNG,
  IMAGE_FORMAT_GIF
};

struct image_info {
  uns width;
  uns height;
  enum image_format format;
  union {
    struct {
    } jpeg;
    struct {
    } png;
    struct {
    } gif;
  };
};

int read_image_header(struct image_info *info);
int read_image_data(struct image_info *info);

#endif

