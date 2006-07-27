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
  int need_scale;
  int need_destroy;
};

static inline struct image *
image_io_read_data_prepare(struct image_io_read_data_internals *rdi, struct image_io *io, uns cols, uns rows)
{
  rdi->need_scale = io->cols != cols | io->rows != rows;
  rdi->need_destroy = rdi->need_scale || !io->pool;
  return rdi->image = rdi->need_scale ?
    image_new(io->thread, cols, rows, io->flags & IMAGE_CHANNELS_FORMAT, NULL) :
    image_new(io->thread, io->cols, io->rows, io->flags & IMAGE_IO_IMAGE_FLAGS, io->pool);
}

static inline int
image_io_read_data_finish(struct image_io_read_data_internals *rdi, struct image_io *io)
{
  if (rdi->need_scale)
    {
      struct image *img = image_new(io->thread, io->cols, io->rows, io->flags & IMAGE_IO_IMAGE_FLAGS, io->pool);
      if (unlikely(!img))
        {
	  if (rdi->need_destroy)
	    image_destroy(rdi->image);
	  return 0;
	}
      if (unlikely(!image_scale(io->thread, img, rdi->image)))
        {
          image_destroy(rdi->image);
	  if (!io->pool)
	    image_destroy(img);
	  return 0;
	}
      io->image = img;
      io->image_destroy = !io->pool;
    }
  else
    {
      io->image = rdi->image;
      io->image_destroy = rdi->need_destroy;
    }
  return 1;
}

static inline void
image_io_read_data_break(struct image_io_read_data_internals *rdi, struct image_io *io UNUSED)
{
  if (rdi->need_destroy)
    image_destroy(rdi->image);
}

#endif
