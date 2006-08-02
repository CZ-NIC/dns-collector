#ifndef _IMAGES_IO_MAIN_H
#define _IMAGES_IO_MAIN_H

int libjpeg_read_header(struct image_io *io);
int libjpeg_read_data(struct image_io *io);
int libjpeg_write(struct image_io *io);

int libpng_read_header(struct image_io *io);
int libpng_read_data(struct image_io *io);
int libpng_write(struct image_io *io);

int libungif_read_header(struct image_io *io);
int libungif_read_data(struct image_io *io);

int libmagick_read_header(struct image_io *io);
int libmagick_read_data(struct image_io *io);
int libmagick_write(struct image_io *io);

struct image_io_read_data_internals {
  struct image *image;
  int need_transformations;
};

struct image *image_io_read_data_prepare(struct image_io_read_data_internals *rdi, struct image_io *io, uns cols, uns rows, uns flags);
int image_io_read_data_finish(struct image_io_read_data_internals *rdi, struct image_io *io);
void image_io_read_data_break(struct image_io_read_data_internals *rdi, struct image_io *io);

#endif
