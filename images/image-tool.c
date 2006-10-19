/*
 *	Image Library -- Simple image manipulation utility
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include "images/color.h"

#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

static void NONRET
usage(void)
{
  fputs("\
Usage: image-tool [options] infile [outfile]\n\
\n\
-q --quiet               no progress messages\n\
-f --input-format        input image format (jpeg, gif, png)\n\
-F --output-format       output image format\n\
-s --size                force output dimensions (100x200)\n\
-b --fit-to-box          scale to fit the box (100x200)\n\
-c --colorspace          force output colorspace (Gray, GrayAlpha, RGB, RGBAlpha)\n\
-Q --jpeg-quality        JPEG quality (1..100)\n\
-g --background          background color (hexadecimal RRGGBB)\n\
-G --default-background  background applied only if the image contains no background info (RRGGBB, default=FFFFFF)\n\
-a --remove-alpha        remove alpha channel\n\
-e --exif                reads Exif data\n"
, stderr);
  exit(1);
}

static char *shortopts = "qf:F:s:b:c:Q:g:G:ae";
static struct option longopts[] =
{
  { "quiet",			0, 0, 'q' },
  { "input-format",		0, 0, 'f' },
  { "output-format",		0, 0, 'F' },
  { "size",			0, 0, 's' },
  { "fit-to-box",		0, 0, 'b' },
  { "colorspace",		0, 0, 'c' },
  { "jpeg-quality",		0, 0, 'Q' },
  { "background",		0, 0, 'g' },
  { "default-background",	0, 0, 'G' },
  { "remove-alpha",		0, 0, 'a' },
  { "exif",			0, 0, 'e' },
  { NULL,			0, 0, 0 }
};

static uns verbose = 1;
static byte *input_file_name;
static enum image_format input_format;
static byte *output_file_name;
static enum image_format output_format;
static uns cols;
static uns rows;
static uns fit_to_box;
static uns channels_format;
static uns jpeg_quality;
static struct color background_color;
static struct color default_background_color;
static uns remove_alpha;
static uns exif;

static void
parse_color(struct color *color, byte *s)
{
  if (strlen(s) != 6)
    usage();
  errno = 0;
  char *end;
  long int v = strtol(s, &end, 16);
  if (errno || *end || v < 0)
    usage();
  color_make_rgb(color, (v >> 16) & 255, (v >> 8) & 255, v & 255);
}

#define MSG(x...) do{ if (verbose) log(L_INFO, ##x); }while(0)

int
main(int argc, char **argv)
{
  log_init(argv[0]);
  int opt;
  default_background_color = color_white;
  while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) >= 0)
    switch (opt)
      {
	case 'q':
	  verbose = 0;
	  break;
	case 'f':
	  if (!(input_format = image_extension_to_format(optarg)))
	    usage();
	  break;
	case 'F':
	  if (!(output_format = image_extension_to_format(optarg)))
	    usage();
	  break;
	case 's':
	  {
	    byte *r = strchr(optarg, 'x');
	    if (!r)
	      usage();
	    *r++ = 0;
	    if (!(cols = atoi(optarg)) || !(rows = atoi(r)))
	      usage();
	    fit_to_box = 0;
	    break;
	  }
	case 'b':
	  {
	    byte *r = strchr(optarg, 'x');
	    if (!r)
	      usage();
	    *r++ = 0;
	    if (!(cols = atoi(optarg)) || !(rows = atoi(r)))
	      usage();
	    fit_to_box = 1;
	    break;
	  }
	case 'c':
	  if (!(channels_format = image_name_to_channels_format(optarg)))
	    usage();
	  break;
	case 'Q':
	  if (!(jpeg_quality = atoi(optarg)))
	    usage();
	  break;
	case 'g':
	  parse_color(&background_color, optarg);
	  break;
	case 'G':
	  parse_color(&default_background_color, optarg);
	  break;
	case 'a':
	  remove_alpha++;
	  break;
	case 'e':
	  exif++;
	  break;
	default:
	  usage();
      }

  if (argc != optind + 1 && argc != optind + 2)
    usage();
  input_file_name = argv[optind++];
  if (argc > optind)
    output_file_name = argv[optind];

#define TRY(x) do{ if (!(x)) exit(1); }while(0)
  MSG("Initializing image library");
  struct image_context ctx;
  struct image_io io;
  image_context_init(&ctx);
  ctx.tracing_level = ~0U;
  if (!image_io_init(&ctx, &io))
    die("Cannot initialize image I/O");

  MSG("Reading %s", input_file_name);
  io.fastbuf = bopen(input_file_name, O_RDONLY, 1 << 18);
  io.format = input_format ? : image_file_name_to_format(input_file_name);
  if (exif)
    io.flags |= IMAGE_IO_WANT_EXIF;
  TRY(image_io_read_header(&io));
  if (!output_file_name)
    {
      bclose(io.fastbuf);
      printf("Format:      %s\n", image_format_to_extension(io.format) ? : (byte *)"?");
      printf("Dimensions:  %dx%d\n", io.cols, io.rows);
      printf("Colorspace:  %s\n", (io.flags & IMAGE_IO_HAS_PALETTE) ? (byte *)"Palette" : image_channels_format_to_name(io.flags & IMAGE_CHANNELS_FORMAT));
      printf("NumColors:   %d\n", io.number_of_colors);
      if (io.background_color.color_space)
        {
	  byte rgb[3];
	  color_put_rgb(rgb, &io.background_color);
          printf("Background:  %02x%02x%02x\n", rgb[0], rgb[1], rgb[2]);
	}
      if (io.exif_size)
	printf("ExifSize:    %u\n", io.exif_size);
    }
  else
    {
      MSG("%s %dx%d %s", image_format_to_extension(io.format) ? : (byte *)"?", io.cols, io.rows,
	  (io.flags & IMAGE_IO_HAS_PALETTE) ? (byte *)"Palette" : image_channels_format_to_name(io.flags & IMAGE_CHANNELS_FORMAT));
      if (cols)
        if (fit_to_box)
	  {
            image_dimensions_fit_to_box(&io.cols, &io.rows, MIN(cols, 0xffff), MIN(rows, 0xffff), 0);
	  }
        else
          {
            io.cols = cols;
            io.rows = rows;
          }
      if (background_color.color_space)
	io.background_color = background_color;
      else if (!io.background_color.color_space)
	io.background_color = default_background_color;
      if (remove_alpha)
	io.flags &= ~IMAGE_ALPHA;
      if (channels_format)
        io.flags = io.flags & ~IMAGE_PIXEL_FORMAT | channels_format;
      if (!(io.flags & IMAGE_ALPHA))
        io.flags |= IMAGE_IO_USE_BACKGROUND;
      if (jpeg_quality)
	io.jpeg_quality = jpeg_quality;
      TRY(image_io_read_data(&io, 0));
      bclose(io.fastbuf);
      MSG("Writing %s", output_file_name);
      io.fastbuf = bopen(output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 1 << 18);
      io.format = output_format ? : image_file_name_to_format(output_file_name);
      MSG("%s %dx%d %s", image_format_to_extension(io.format) ? : (byte *)"?", io.cols, io.rows,
	  image_channels_format_to_name(io.flags & IMAGE_CHANNELS_FORMAT));
      TRY(image_io_write(&io));
      bclose(io.fastbuf);
    }

  image_io_cleanup(&io);
  image_context_cleanup(&ctx);
  MSG("Done.");
  return 0;
}
