/*
 *	Simple image manupulation utility
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
#include <stdlib.h>
#include <fcntl.h>

static void NONRET
usage(void)
{
  fputs("\
Usage: image-tool [options] infile [outfile]\n\
\n\
-q --quiet          no progress messages\n\
-f --input-format   input image format (jpeg, gif, png)\n\
-F --output-format  output image format\n\
-s --size           force output dimensions (100x200)\n\
-b --fit-box        scale to fit the box (100x200)\n\
-c --colorspace     force output colorspace (gray, grayalpha, rgb, rgbalpha)\n\
", stderr);
  exit(1);
}

static char *shortopts = "qf:F:s:b:c:" CF_SHORT_OPTS;
static struct option longopts[] =
{
  CF_LONG_OPTS
  { "quiet",		0, 0, 'q' },
  { "input-format",	0, 0, 'f' },
  { "output-format",	0, 0, 'F' },
  { "size",		0, 0, 's' },
  { "fit-box",		0, 0, 'b' },
  { "colorspace",	0, 0, 'c' },
  { NULL,		0, 0, 0 }
};
							  
static uns verbose = 1;
static byte *input_file_name;
static enum image_format input_format;
static byte *output_file_name;
static enum image_format output_format;
static uns cols;
static uns rows;
static uns fit_box;
static uns channels_format;

#define MSG(x...) do{ if (verbose) log(L_INFO, ##x); }while(0)

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
	    fit_box = 0;
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
	    fit_box = 1;
	    break;
	  }
	case 'c':
	  if (!(channels_format = image_name_to_channels_format(optarg)))
	    usage();
	  break;
	default:
	  usage();
      }

  if (argc != optind + 1 && argc != optind + 2)
    usage();
  input_file_name = argv[optind++];
  if (argc > optind)
    output_file_name = argv[optind];
  
#define TRY(x) do{ if (!(x)) die("Error: %s", it.err_msg); }while(0)
  MSG("Initializing image library");
  struct image_thread it;
  struct image_io io;
  image_thread_init(&it);
  image_io_init(&it, &io);

  MSG("Reading %s", input_file_name);
  io.fastbuf = bopen(input_file_name, O_RDONLY, 1 << 18);
  io.format = input_format ? : image_file_name_to_format(input_file_name);
  TRY(image_io_read_header(&io));
  if (!output_file_name)
    {
      bclose(io.fastbuf);
      printf("Format:      %s\n", image_format_to_extension(io.format) ? : (byte *)"?");
      printf("Dimensions:  %dx%d\n", io.cols, io.rows);
      printf("Colorspace:  %s\n", image_channels_format_to_name(io.flags & IMAGE_CHANNELS_FORMAT));
    }
  else
    {
      MSG("%s %dx%d %s", image_format_to_extension(io.format) ? : (byte *)"?", io.cols, io.rows,
	  image_channels_format_to_name(io.flags & IMAGE_CHANNELS_FORMAT));
      if (cols)
        if (fit_box)
	  {
            image_dimensions_fit_to_box(&io.cols, &io.rows, MIN(cols, 0xffff), MIN(rows, 0xffff), 0);
	  }
        else
          {
            io.cols = cols;
            io.rows = rows;
          }
      if (channels_format)
        io.flags = io.flags & ~IMAGE_PIXEL_FORMAT | channels_format;
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
  image_thread_cleanup(&it);
  MSG("Done.");
  return 0;
}
