#ifndef _IMAGES_IO_MAIN_H
#define _IMAGES_IO_MAIN_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define image_io_read_data_break ucw_image_io_read_data_break
#define image_io_read_data_finish ucw_image_io_read_data_finish
#define image_io_read_data_prepare ucw_image_io_read_data_prepare
#define libjpeg_read_data ucw_libjpeg_read_data
#define libjpeg_read_header ucw_libjpeg_read_header
#define libjpeg_write ucw_libjpeg_write
#define libmagick_cleanup ucw_libmagick_cleanup
#define libmagick_init ucw_libmagick_init
#define libmagick_read_data ucw_libmagick_read_data
#define libmagick_read_header ucw_libmagick_read_header
#define libmagick_write ucw_libmagick_write
#define libpng_read_data ucw_libpng_read_data
#define libpng_read_header ucw_libpng_read_header
#define libpng_write ucw_libpng_write
#define libungif_read_data ucw_libungif_read_data
#define libungif_read_header ucw_libungif_read_header
#endif

static inline int libjpeg_init(struct image_io *io UNUSED) { return 1; }
static inline void libjpeg_cleanup(struct image_io *io UNUSED) {}
int libjpeg_read_header(struct image_io *io);
int libjpeg_read_data(struct image_io *io);
int libjpeg_write(struct image_io *io);

static inline int libpng_init(struct image_io *io UNUSED) { return 1; }
static inline void libpng_cleanup(struct image_io *io UNUSED) {}
int libpng_read_header(struct image_io *io);
int libpng_read_data(struct image_io *io);
int libpng_write(struct image_io *io);

static inline int libungif_init(struct image_io *io UNUSED) { return 1; }
static inline void libungif_cleanup(struct image_io *io UNUSED) {}
int libungif_read_header(struct image_io *io);
int libungif_read_data(struct image_io *io);

int libmagick_init(struct image_io *io);
void libmagick_cleanup(struct image_io *io);
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
