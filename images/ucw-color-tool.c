/*
 *	Color spaces tool
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License.
 */

#include <ucw/lib.h>
#include <images/images.h>
#include <images/color.h>

#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

static void NONRET
usage(void)
{
  fputs("\
Usage: ucw-color-tool input-color-space output-color-space\n\
", stderr);
  exit(1);
}

static char *shortopts = "";
static struct option longopts[] =
{
  { NULL,			0, 0, 0 }
};

static const struct color_space_info *
parse_color_space(byte *s)
{
  if (!strcasecmp(s, "sRGB"))
    return &color_srgb_info;
  else if (!strcasecmp(s, "AdobeRGB") || !strcasecmp(s, "Adobe RGB"))
    return &color_adobe_rgb_info;
  else if (!strcasecmp(s, "CIERGB") || strcasecmp(s, "CIE RGB"))
    return &color_cie_rgb_info;
  else
    die("Unknown color space");
}

static void
print_matrix(double m[9])
{
  for (uns j = 0; j < 3; j++)
    {
      for (uns i = 0; i < 3; i++)
	printf(" %12.8f", m[i + j * 3]);
      printf("\n");
    }
}

int
main(int argc, char **argv)
{
  log_init(argv[0]);
  int opt;
  while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) >= 0)
    switch (opt)
      {
	default:
	  usage();
      }

  if (argc == optind + 1)
    {
      const struct color_space_info *a = parse_color_space(argv[optind]);
      double a_to_xyz[9], xyz_to_a[9];
      color_compute_color_space_to_xyz_matrix(a_to_xyz, &a->chromacity);
      color_invert_matrix(xyz_to_a, a_to_xyz);
      printf("linear %s -> XYZ:\n", a->name);
      print_matrix(a_to_xyz);
      printf("XYZ -> linear %s:\n", a->name);
      print_matrix(xyz_to_a);
      printf("Simple gamma: %.8f\n", a->gamma.simple_gamma);
      printf("Detailed gamma: g=%.8f o=%.8f t=%.8f s=%.8f\n", a->gamma.detailed_gamma, a->gamma.offset, a->gamma.transition, a->gamma.slope);
    }
  else if (argc == optind + 2)
    {
      const struct color_space_info *a = parse_color_space(argv[optind++]);
      const struct color_space_info *b = parse_color_space(argv[optind]);
      double a_to_b[9];
      color_compute_color_spaces_conversion_matrix(a_to_b, &a->chromacity, &b->chromacity);
      printf("linear %s -> linear %s:\n", a->name, b->name);
      print_matrix(a_to_b);
    }
  else
    usage();

  return 0;
}
