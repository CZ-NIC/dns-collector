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
Usage: image-sim-test [options] image1 image2 \n\
\n\
-q --quiet           no progress messages\n\
-f --format-1        image1 format (jpeg, gif, png)\n\
-F --format-2        image2 format\n\
-g --background      background color (hexadecimal RRGGBB)\n\
", stderr);
  exit(1);
}

static char *shortopts = "qf:F:g:t:" CF_SHORT_OPTS;
static struct option longopts[] =
{
  CF_LONG_OPTS
  { "quiet",		0, 0, 'q' },
  { "format-1",		0, 0, 'f' },
  { "format-2",		0, 0, 'F' },
  { "background",	0, 0, 'g' },
  { NULL,		0, 0, 0 }
};
							  
static uns verbose = 1;
static byte *file_name_1;
static byte *file_name_2;
static enum image_format format_1;
static enum image_format format_2;
static struct color background_color;

#define MSG(x...) do{ if (verbose) log(L_INFO, ##x); }while(0)

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
	default:
	  usage();
      }

  if (argc != optind + 2)
    usage();
  file_name_1 = argv[optind++];
  file_name_2 = argv[optind];
  
#define TRY(x) do{ if (!(x)) die("Error: %s", it.err_msg); }while(0)
  MSG("Initializing image library");
  srandom(time(NULL) ^ getpid());
  srgb_to_luv_init();
  struct image_thread it;
  struct image_io io;
  image_thread_init(&it);

  struct image *img1, *img2;

  image_io_init(&it, &io);
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
  image_io_cleanup(&io);
  MSG("Image size=%ux%u", img2->cols, img2->rows);

  MSG("Computing signatures");
  struct image_signature sig1, sig2;
  TRY(compute_image_signature(&it, &sig1, img1));
  TRY(compute_image_signature(&it, &sig2, img2));
  dump_signature(&sig1);
  dump_signature(&sig2);

  uns dist = image_signatures_dist(&sig1, &sig1);
  MSG("dist=%.6f", dist / (double)(1 << IMAGE_SIG_DIST_SCALE));
  
  image_destroy(img1);
  image_destroy(img2);
  image_thread_cleanup(&it);
  MSG("Done.");
  return 0;
}
