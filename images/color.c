/*
 *	Image Library -- Color Spaces
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/math.h"
#include "images/images.h"
#include "images/color.h"

struct color color_black = { .color_space = COLOR_SPACE_GRAYSCALE };
struct color color_white = { .c = { 255 }, .color_space = COLOR_SPACE_GRAYSCALE };

inline void
color_put_grayscale(byte *dest, struct color *color)
{
  switch (color->color_space)
    {
      case COLOR_SPACE_GRAYSCALE:
	dest[0] = color->c[0];
	break;
      case COLOR_SPACE_RGB:
	dest[0] = rgb_to_gray_func(color->c[0], color->c[1], color->c[2]);
	break;
      default:
	ASSERT(0);	
    }
}

inline void
color_put_rgb(byte *dest, struct color *color)
{
  switch (color->color_space)
    {
      case COLOR_SPACE_GRAYSCALE:
	dest[0] = dest[1] = dest[2] = color->c[0];
	break;
      case COLOR_SPACE_RGB:
	dest[0] = color->c[0];
	dest[1] = color->c[1];
	dest[2] = color->c[2];
	break;
      default:
	ASSERT(0);	
    }
}

void
color_put_color_space(byte *dest, struct color *color, enum color_space color_space)
{
  switch (color_space)
    {
      case COLOR_SPACE_GRAYSCALE:
	color_put_grayscale(dest, color);
	break;  
      case COLOR_SPACE_RGB:
	color_put_rgb(dest, color);	
	break;
      default:
	ASSERT(0);	
    }
}

/********************* EXACT CONVERSION ROUTINES **********************/

/* sRGB to XYZ */
void
srgb_to_xyz_slow(double xyz[3], double srgb[3])
{
  double a[3];
  for (uns i = 0; i < 3; i++)
    if (srgb[i] > 0.04045)
      a[i] = pow((srgb[i] + 0.055) * (1 / 1.055), 2.4);
    else
      a[i] = srgb[i] * (1 / 12.92);
  xyz[0] = SRGB_XYZ_XR * a[0] + SRGB_XYZ_XG * a[1] + SRGB_XYZ_XB * a[2];
  xyz[1] = SRGB_XYZ_YR * a[0] + SRGB_XYZ_YG * a[1] + SRGB_XYZ_YB * a[2];
  xyz[2] = SRGB_XYZ_ZR * a[0] + SRGB_XYZ_ZG * a[1] + SRGB_XYZ_ZB * a[2];
}

/* XYZ to CIE-Luv */
void
xyz_to_luv_slow(double luv[3], double xyz[3])
{
  double sum = xyz[0] + 15 * xyz[1] + 3 * xyz[2];
  if (sum < 0.000001)
    luv[0] = luv[1] = luv[2] = 0;
  else
    {
      double var_u = 4 * xyz[0] / sum;
      double var_v = 9 * xyz[1] / sum;
      if (xyz[1] > 0.008856)
        luv[0] = 116 * pow(xyz[1], 1 / 3.) - 16;
      else
        luv[0] = (116 * 7.787) * xyz[1];
     luv[1] = luv[0] * (13 * (var_u - 4 * REF_WHITE_X / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z)));
     luv[2] = luv[0] * (13 * (var_v - 9 * REF_WHITE_Y / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z)));
     /* intervals [0..100], [-134..220], [-140..122] */
   }
}


/***************** OPTIMIZED SRGB -> LUV CONVERSION *********************/

u16 srgb_to_luv_tab1[256];
u16 srgb_to_luv_tab2[9 << SRGB_TO_LUV_TAB2_SIZE];
u32 srgb_to_luv_tab3[20 << SRGB_TO_LUV_TAB3_SIZE];

void
srgb_to_luv_init(void)
{
  DBG("Initializing sRGB -> Luv table");
  for (uns i = 0; i < 256; i++)
    {
      double t = i / 255.;
      if (t > 0.04045)
        t = pow((t + 0.055) * (1 / 1.055), 2.4);
      else
        t = t * (1 / 12.92);
      srgb_to_luv_tab1[i] = CLAMP(t * 0xfff + 0.5, 0, 0xfff);
    }
  for (uns i = 0; i < (9 << SRGB_TO_LUV_TAB2_SIZE); i++)
    {
      double t = i / (double)((9 << SRGB_TO_LUV_TAB2_SIZE) - 1);
      if (t > 0.008856)
        t = 1.16 * pow(t, 1 / 3.) - 0.16;
      else
        t = (1.16 * 7.787) * t;
      srgb_to_luv_tab2[i] =
	CLAMP(t * ((1 << SRGB_TO_LUV_TAB2_SCALE) - 1) + 0.5,
          0, (1 << SRGB_TO_LUV_TAB2_SCALE) - 1);
    }
  for (uns i = 0; i < (20 << SRGB_TO_LUV_TAB3_SIZE); i++)
    {
      srgb_to_luv_tab3[i] = i ? (13 << (SRGB_TO_LUV_TAB3_SCALE + SRGB_TO_LUV_TAB3_SIZE)) / i : 0;
    }
}

void
srgb_to_luv_pixels(byte *dest, byte *src, uns count)
{
  while (count--)
    {
      srgb_to_luv_pixel(dest, src);
      dest += 3;
      src += 3;
    }
}


/************************ GRID INTERPOLATION ALGORITHM ************************/

struct color_grid_node *srgb_to_luv_grid;
struct color_interpolation_node *color_interpolation_table;

/* Returns volume of a given tetrahedron multiplied by 6 */
static inline uns
tetrahedron_volume(uns *v1, uns *v2, uns *v3, uns *v4)
{
  int a[3], b[3], c[3];
  for (uns i = 0; i < 3; i++)
    {
      a[i] = v2[i] - v1[i];
      b[i] = v3[i] - v1[i];
      c[i] = v4[i] - v1[i];
    }
  int result =
    a[0] * (b[1] * c[2] - b[2] * c[1]) -
    a[1] * (b[0] * c[2] - b[2] * c[0]) +
    a[2] * (b[0] * c[1] - b[1] * c[0]);
  return (result > 0) ? result : -result;
}

static void
interpolate_tetrahedron(struct color_interpolation_node *n, uns *p, const uns *c)
{
  uns v[4][3];
  for (uns i = 0; i < 4; i++)
    {
      v[i][0] = (c[i] & 0001) ? (1 << COLOR_CONV_OFS) : 0;
      v[i][1] = (c[i] & 0010) ? (1 << COLOR_CONV_OFS) : 0;
      v[i][2] = (c[i] & 0100) ? (1 << COLOR_CONV_OFS) : 0;
      n->ofs[i] =
	((c[i] & 0001) ? 1 : 0) +
	((c[i] & 0010) ? (1 << COLOR_CONV_SIZE) : 0) +
	((c[i] & 0100) ? (1 << (COLOR_CONV_SIZE * 2)) : 0);
    }
  uns vol = tetrahedron_volume(v[0], v[1], v[2], v[3]);
  n->mul[0] = ((tetrahedron_volume(p, v[1], v[2], v[3]) << 8) + (vol >> 1)) / vol;
  n->mul[1] = ((tetrahedron_volume(v[0], p, v[2], v[3]) << 8) + (vol >> 1)) / vol;
  n->mul[2] = ((tetrahedron_volume(v[0], v[1], p, v[3]) << 8) + (vol >> 1)) / vol;
  n->mul[3] = ((tetrahedron_volume(v[0], v[1], v[2], p) << 8) + (vol >> 1)) / vol;
  uns j;
  for (j = 0; j < 4; j++)
    if (n->mul[j])
      break;
  for (uns i = 0; i < 4; i++)
    if (n->mul[i] == 0)
      n->ofs[i] = n->ofs[j];
}

static void
interpolation_table_init(void)
{
  DBG("Initializing color interpolation table");
  struct color_interpolation_node *n = color_interpolation_table =
    xmalloc(sizeof(struct color_interpolation_node) << (COLOR_CONV_OFS * 3));
  uns p[3];
  for (p[2] = 0; p[2] < (1 << COLOR_CONV_OFS); p[2]++)
    for (p[1] = 0; p[1] < (1 << COLOR_CONV_OFS); p[1]++)
      for (p[0] = 0; p[0] < (1 << COLOR_CONV_OFS); p[0]++)
        {
	  uns index;
          static const uns tetrahedrons[5][4] = {
            {0000, 0001, 0010, 0100},
            {0110, 0111, 0100, 0010},
            {0101, 0100, 0111, 0001},
            {0011, 0010, 0001, 0111},
            {0111, 0001, 0010, 0100}};
	  if (p[0] + p[1] + p[2] <= (1 << COLOR_CONV_OFS))
	    index = 0;
	  else if ((1 << COLOR_CONV_OFS) + p[0] <= p[1] + p[2])
	    index = 1;
	  else if ((1 << COLOR_CONV_OFS) + p[1] <= p[0] + p[2])
	    index = 2;
	  else if ((1 << COLOR_CONV_OFS) + p[2] <= p[0] + p[1])
	    index = 3;
	  else
	    index = 4;
	  interpolate_tetrahedron(n, p, tetrahedrons[index]);
	  n++;
	}
}

typedef void color_conv_func(double dest[3], double src[3]);

static void
conv_grid_init(struct color_grid_node **grid, color_conv_func func)
{
  if (*grid)
    return;
  struct color_grid_node *g = *grid = xmalloc((sizeof(struct color_grid_node)) << (COLOR_CONV_SIZE * 3));
  double src[3], dest[3];
  for (uns k = 0; k < (1 << COLOR_CONV_SIZE); k++)
    {
      src[2] = k * (255 / (double)((1 << COLOR_CONV_SIZE) - 1));
      for (uns j = 0; j < (1 << COLOR_CONV_SIZE); j++)
        {
          src[1] = j * (255/ (double)((1 << COLOR_CONV_SIZE) - 1));
          for (uns i = 0; i < (1 << COLOR_CONV_SIZE); i++)
            {
              src[0] = i * (255 / (double)((1 << COLOR_CONV_SIZE) - 1));
	      func(dest, src);
	      g->val[0] = CLAMP(dest[0] + 0.5, 0, 255);
	      g->val[1] = CLAMP(dest[1] + 0.5, 0, 255);
	      g->val[2] = CLAMP(dest[2] + 0.5, 0, 255);
	      g++;
	    }
	}
    }
}

static void
srgb_to_luv_func(double dest[3], double src[3])
{
  double srgb[3], xyz[3], luv[3];
  srgb[0] = src[0] / 255.;
  srgb[1] = src[1] / 255.;
  srgb[2] = src[2] / 255.;
  srgb_to_xyz_slow(xyz, srgb);
  xyz_to_luv_slow(luv, xyz);
  dest[0] = luv[0] * 2.55;
  dest[1] = luv[1] * (2.55 / 4) + 128;
  dest[2] = luv[2] * (2.55 / 4) + 128;
}

void
color_conv_init(void)
{
  interpolation_table_init();
  conv_grid_init(&srgb_to_luv_grid, srgb_to_luv_func);
}

void
color_conv_pixels(byte *dest, byte *src, uns count, struct color_grid_node *grid)
{
  while (count--)
    {
      color_conv_pixel(dest, src, grid);
      dest += 3;
      src += 3;
    }
}


/**************************** TESTS *******************************/

#ifdef TEST
#include <string.h>

static double
conv_error(u32 color, struct color_grid_node *grid, color_conv_func func)
{
  byte src[3], dest[3];
  src[0] = color & 255;
  src[1] = (color >> 8) & 255;
  src[2] = (color >> 16) & 255;
  color_conv_pixel(dest, src, grid);
  double src2[3], dest2[3];
  for (uns i = 0; i < 3; i++)
    src2[i] = src[i];
  func(dest2, src2);
  double err = 0;
  for (uns i = 0; i < 3; i++)
    err += (dest[i] - dest2[i]) * (dest[i] - dest2[i]);
  return err;
}

typedef void test_fn(byte *dest, byte *src);

static double
func_error(u32 color, test_fn test, color_conv_func func)
{
  byte src[3], dest[3];
  src[0] = color & 255;
  src[1] = (color >> 8) & 255;
  src[2] = (color >> 16) & 255;
  test(dest, src);
  double src2[3], dest2[3];
  for (uns i = 0; i < 3; i++)
    src2[i] = src[i];
  func(dest2, src2);
  double err = 0;
  for (uns i = 0; i < 3; i++)
    err += (dest[i] - dest2[i]) * (dest[i] - dest2[i]);
  return err;
}

static void
test_grid(byte *name, struct color_grid_node *grid, color_conv_func func)
{
  double max_err = 0, sum_err = 0;
  uns count = 100000;
  for (uns i = 0; i < count; i++)
    {
      double err = conv_error(random_max(0x1000000), grid, func);
      max_err = MAX(err, max_err);
      sum_err += err;
    }
  DBG("%s: error max=%f avg=%f", name, max_err, sum_err / count);
  if (max_err > 12)
    die("Too large error in %s conversion", name);
}

static void
test_func(byte *name, test_fn test, color_conv_func func)
{
  double max_err = 0, sum_err = 0;
  uns count = 100000;
  for (uns i = 0; i < count; i++)
    {
      double err = func_error(random_max(0x1000000), test, func);
      max_err = MAX(err, max_err);
      sum_err += err;
    }
  DBG("%s: error max=%f avg=%f", name, max_err, sum_err / count);
  if (max_err > 12)
    die("Too large error in %s conversion", name);
}

int
main(void)
{
  srgb_to_luv_init();
  test_func("func sRGB -> Luv", srgb_to_luv_pixel, srgb_to_luv_func);
  color_conv_init();
  test_grid("grid sRGB -> Luv", srgb_to_luv_grid, srgb_to_luv_func);
#ifdef LOCAL_DEBUG
#define CNT 1000000
#define TESTS 10
  byte *a = xmalloc(3 * CNT), *b = xmalloc(3 * CNT);
  for (uns i = 0; i < 3 * CNT; i++)
    a[i] = random_max(256);
  init_timer();
  for (uns i = 0; i < TESTS; i++)
    memcpy(b, a, CNT * 3);
  DBG("memcpy time=%d", (uns)get_timer());
  init_timer();
  for (uns i = 0; i < TESTS; i++)
    srgb_to_luv_pixels(b, a, CNT);
  DBG("direct time=%d", (uns)get_timer());
  init_timer();
  for (uns i = 0; i < TESTS; i++)
    color_conv_pixels(b, a, CNT, srgb_to_luv_grid);
  DBG("grid time=%d", (uns)get_timer());
#endif
  return 0;
}
#endif

