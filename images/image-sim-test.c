/*
 *	Image similarity testing
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License.
 */

#include "lib/lib.h"
#include "lib/getopt.h"
#include "lib/fastbuf.h"
#include "lib/base64.h"
#include "images/images.h"
#include "images/color.h"
#include "images/signature.h"

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

static void NONRET
usage(void)
{
  fputs("\
Usage: image-sim-test [options] image1 [image2] \n\
\n\
-q --quiet           no progress messages\n\
-f --format-1        image1 format (jpeg, gif, png)\n\
-F --format-2        image2 format\n\
-g --background      background color (hexadecimal RRGGBB)\n\
-r --segmentation-1  writes image1 segmentation to given file\n\
-R --segmentation-2  writes image2 segmentation to given file\n\
-6 --base64          display base64 encoded signature\n\
", stderr);
  exit(1);
}

static char *shortopts = "qf:F:g:t:r:R:6" CF_SHORT_OPTS;
static struct option longopts[] =
{
  CF_LONG_OPTS
  { "quiet",		0, 0, 'q' },
  { "format-1",		0, 0, 'f' },
  { "format-2",		0, 0, 'F' },
  { "background",	0, 0, 'g' },
  { "segmentation-1",	0, 0, 'r' },
  { "segmentation-2",	0, 0, 'R' },
  { "base64",		0, 0, '6' },
  { NULL,		0, 0, 0 }
};

static uns verbose = 1;
static byte *file_name_1;
static byte *file_name_2;
static enum image_format format_1;
static enum image_format format_2;
static struct color background_color;
static byte *segmentation_name_1;
static byte *segmentation_name_2;
static uns display_base64;

#define MSG(x...) do{ if (verbose) log(L_INFO, ##x); }while(0)
#define TRY(x) do{ if (!(x)) exit(1); }while(0)

static void
msg_str(byte *s, void *param UNUSED)
{
  MSG("%s", s);
}

static void
dump_signature(struct image_signature *sig)
{
  byte buf[MAX(IMAGE_VECTOR_DUMP_MAX, IMAGE_REGION_DUMP_MAX)];
  image_vector_dump(buf, &sig->vec);
  MSG("vector: %s", buf);
  for (uns i = 0; i < sig->len; i++)
    {
      image_region_dump(buf, sig->reg + i);
      MSG("region %u: %s", i, buf);
    }
  if (display_base64)
    {
      uns sig_size = image_signature_size(sig->len);
      byte buf[BASE64_ENC_LENGTH(sig_size) + 1];
      uns enc_size = base64_encode(buf, (byte *)sig, sig_size);
      buf[enc_size] = 0;
      MSG("base64 encoded: %s", buf);
    }
}

static struct image_context ctx;
static struct image_io io;

static void
write_segmentation(struct image_sig_data *data, byte *fn)
{
  MSG("Writing segmentation to %s", fn);
  
  struct fastbuf *fb = bopen(fn, O_WRONLY | O_CREAT | O_TRUNC, 4096);
  struct image *img;
  TRY(img = image_new(&ctx, data->image->cols, data->image->rows, COLOR_SPACE_RGB, NULL));
  image_clear(&ctx, img);

  for (uns i = 0; i < data->regions_count; i++)
    {
      byte c[3];
      double luv[3], xyz[3], srgb[3];
      luv[0] = data->regions[i].a[0] * (4 / 2.55);
      luv[1] = ((int)data->regions[i].a[1] - 128) * (4 / 2.55);
      luv[2] = ((int)data->regions[i].a[2] - 128) * (4 / 2.55);
      luv_to_xyz_exact(xyz, luv);
      xyz_to_srgb_exact(srgb, xyz);
      c[0] = CLAMP(srgb[0] * 255, 0, 255); 
      c[1] = CLAMP(srgb[1] * 255, 0, 255); 
      c[2] = CLAMP(srgb[2] * 255, 0, 255); 
      for (struct image_sig_block *block = data->regions[i].blocks; block; block = block->next)
        {
	  uns x1 = block->x * 4;
	  uns y1 = block->y * 4;
	  uns x2 = MIN(x1 + 4, img->cols);
	  uns y2 = MIN(y1 + 4, img->rows);
	  byte *p = img->pixels + x1 * 3 + y1 * img->row_size;
	  for (uns y = y1; y < y2; y++, p += img->row_size)
	    {
	      byte *p2 = p;
	      for (uns x = x1; x < x2; x++, p2 += 3)
	        {
	          p2[0] = c[0];
	          p2[1] = c[1];
	          p2[2] = c[2];
	        }
	    }
        }
    }

  io.fastbuf = fb;
  io.image = img;
  io.format = image_file_name_to_format(fn); 
  TRY(image_io_write(&io));
  image_io_reset(&io);

  image_destroy(img);
  bclose(fb);
}

int
main(int argc, char **argv)
{
  log_init(argv[0]);
  int opt;
  while ((opt = cf_getopt(argc, argv, shortopts, longopts, NULL)) >= 0)
    switch (opt)
      {
	case 'q':
	  verbose = 0;
	  break;
	case 'f':
	  if (!(format_1 = image_extension_to_format(optarg)))
	    usage();
	  break;
	case 'F':
	  if (!(format_2 = image_extension_to_format(optarg)))
	    usage();
	  break;
	case 'g':
	  {
	    if (strlen(optarg) != 6)
	      usage();
	    errno = 0;
	    char *end;
	    long int v = strtol(optarg, &end, 16);
	    if (errno || *end || v < 0)
	      usage();
	    color_make_rgb(&background_color, (v >> 16) & 255, (v >> 8) & 255, v & 255);
	  }
	  break;
	case 'r':
	  segmentation_name_1 = optarg;
	  break;
	case 'R':
	  segmentation_name_2 = optarg;
	  break;
	case '6':
	  display_base64++;
	  break;
	default:
	  usage();
      }

  if (argc != optind + 2 && argc != optind + 1)
    usage();
  file_name_1 = argv[optind++];
  if (argc > optind)
    file_name_2 = argv[optind++];

  MSG("Initializing image library");
  srandom(time(NULL) ^ getpid());
  srgb_to_luv_init();
  image_context_init(&ctx);

  struct image *img1, *img2;

  TRY(image_io_init(&ctx, &io));

  if (file_name_1)
    {
      MSG("Reading %s", file_name_1);
      io.fastbuf = bopen(file_name_1, O_RDONLY, 1 << 18);
      io.format = format_1 ? : image_file_name_to_format(file_name_1);
      TRY(image_io_read_header(&io));
      io.flags = COLOR_SPACE_RGB | IMAGE_IO_USE_BACKGROUND;
      if (background_color.color_space)
        io.background_color = background_color;
      else if (!io.background_color.color_space)
        io.background_color = color_black;
      TRY(image_io_read_data(&io, 1));
      bclose(io.fastbuf);
      img1 = io.image;
      MSG("Image size=%ux%u", img1->cols, img1->rows);
      image_io_reset(&io);
    }
  else
    img1 = NULL;

  if (file_name_2)
    {
      MSG("Reading %s", file_name_2);
      io.fastbuf = bopen(file_name_2, O_RDONLY, 1 << 18);
      io.format = format_2 ? : image_file_name_to_format(file_name_2);
      TRY(image_io_read_header(&io));
      io.flags = COLOR_SPACE_RGB | IMAGE_IO_USE_BACKGROUND;
      if (background_color.color_space)
        io.background_color = background_color;
      else if (!io.background_color.color_space)
        io.background_color = color_black;
      TRY(image_io_read_data(&io, 1));
      bclose(io.fastbuf);
      img2 = io.image;
      MSG("Image size=%ux%u", img2->cols, img2->rows);
      image_io_reset(&io);
    }
  else
    img2 = NULL;

  struct image_signature sig1, sig2;
  MSG("Computing signatures");
  if (img1)
    {
      struct image_sig_data data;
      TRY(image_sig_init(&ctx, &data, img1));
      image_sig_preprocess(&data);
      if (data.valid)
        {
	  image_sig_segmentation(&data);
	  image_sig_detect_textured(&data);
	}
      if (segmentation_name_1)
	write_segmentation(&data, segmentation_name_1);
      image_sig_finish(&data, &sig1);
      image_sig_cleanup(&data);
      dump_signature(&sig1);
    }
  if (img2)
    {
      struct image_sig_data data;
      TRY(image_sig_init(&ctx, &data, img2));
      image_sig_preprocess(&data);
      if (data.valid)
        {
	  image_sig_segmentation(&data);
	  image_sig_detect_textured(&data);
	}
      if (segmentation_name_2)
	write_segmentation(&data, segmentation_name_2);
      image_sig_finish(&data, &sig2);
      image_sig_cleanup(&data);
      dump_signature(&sig2);
    }

  if (img1 && img2)
    {
      uns dist;
      if (verbose)
        {
          struct fastbuf *fb = bfdopen(0, 4096);
          dist = image_signatures_dist_explain(&sig1, &sig2, msg_str, NULL);
          bclose(fb);
	}
      else
	dist = image_signatures_dist(&sig1, &sig2);
      MSG("dist=%u", dist);
    }

  if (img1)
    image_destroy(img1);
  if (img2)
    image_destroy(img2);

  image_io_cleanup(&io);
  image_context_cleanup(&ctx);
  MSG("Done.");
  return 0;
}
