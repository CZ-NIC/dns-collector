#ifndef _IMAGES_IMAGES_H
#define _IMAGES_IMAGES_H

enum image_flag {
  IMAGE_GRAYSCALE = 0x1,	/* grayscale image */
};

struct image {
  uns flags;			/* enum image_flag */
  uns width;			/* number of columns */
  uns height;			/* number of rows */
  byte *pixels;			/* RGB */
};

#endif

