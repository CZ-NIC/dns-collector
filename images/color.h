/*
 *	Image Library -- Color Spaces
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 *
 *
 *	References:
 *	- A Review of RGB Color Spaces, Danny Pascale (2003)
 *	- http://www.adobe.com/digitalimag/pdfs/AdobeRGB1998.pdf
 *	- http://www.tecgraf.puc-rio.br/~mgattass/color/ColorIndex.html
 *
 *	FIXME:
 *	- fix theoretical problems with rounding errors in srgb_to_luv_pixel()
 *	- SIMD should help to speed up conversion of large arrays
 *	- maybe try to generate a long switch in color_conv_pixel()
 *	  with optimized entries instead of access to interpolation table
 *	- most of multiplications in srgb_to_luv_pixels can be replaced
 *	  with tables lookup... tests shows almost the same speed for random
 *	  input and cca 40% gain when input colors fit in CPU chache
 */

#ifndef _IMAGES_COLOR_H
#define _IMAGES_COLOR_H

#include <images/images.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define cmyk_to_rgb_exact ucw_cmyk_to_rgb_exact
#define color_adobe_rgb_info ucw_color_adobe_rgb_info
#define color_apple_rgb_info ucw_color_apple_rgb_info
#define color_black ucw_color_black
#define color_cie_rgb_info ucw_color_cie_rgb_info
#define color_color_match_rgb_info ucw_color_color_match_rgb_info
#define color_compute_bradford_matrix ucw_color_compute_bradford_matrix
#define color_compute_color_space_to_xyz_matrix ucw_color_compute_color_space_to_xyz_matrix
#define color_compute_color_spaces_conversion_matrix ucw_color_compute_color_spaces_conversion_matrix
#define color_conv_init ucw_color_conv_init
#define color_conv_pixels ucw_color_conv_pixels
#define color_get ucw_color_get
#define color_illuminant_d50 ucw_color_illuminant_d50
#define color_illuminant_d65 ucw_color_illuminant_d65
#define color_illuminant_e ucw_color_illuminant_e
#define color_interpolation_table ucw_color_interpolation_table
#define color_invert_matrix ucw_color_invert_matrix
#define color_put ucw_color_put
#define color_space_channels ucw_color_space_channels
#define color_space_id_to_name ucw_color_space_id_to_name
#define color_space_name ucw_color_space_name
#define color_space_name_to_id ucw_color_space_name_to_id
#define color_srgb_info ucw_color_srgb_info
#define color_white ucw_color_white
#define image_conv ucw_image_conv
#define image_conv_defaults ucw_image_conv_defaults
#define luv_to_xyz_exact ucw_luv_to_xyz_exact
#define rgb_to_cmyk_exact ucw_rgb_to_cmyk_exact
#define srgb_to_luv_grid ucw_srgb_to_luv_grid
#define srgb_to_luv_init ucw_srgb_to_luv_init
#define srgb_to_luv_pixels ucw_srgb_to_luv_pixels
#define srgb_to_luv_tab1 ucw_srgb_to_luv_tab1
#define srgb_to_luv_tab2 ucw_srgb_to_luv_tab2
#define srgb_to_luv_tab3 ucw_srgb_to_luv_tab3
#define srgb_to_xyz_exact ucw_srgb_to_xyz_exact
#define xyz_to_luv_exact ucw_xyz_to_luv_exact
#define xyz_to_srgb_exact ucw_xyz_to_srgb_exact
#endif

/* Basic color spaces */
enum {
  COLOR_SPACE_UNKNOWN = 0,
  COLOR_SPACE_UNKNOWN_1 = 1,	/* unknown 1-channel color space */
  COLOR_SPACE_UNKNOWN_2 = 2,	/* unknown 2-channels color space */
  COLOR_SPACE_UNKNOWN_3 = 3,	/* unknown 3-channels color space */
  COLOR_SPACE_UNKNOWN_4 = 4,	/* unknown 4-channels color space */
  COLOR_SPACE_UNKNOWN_MAX = 4,
  COLOR_SPACE_GRAYSCALE,
  COLOR_SPACE_RGB,
  COLOR_SPACE_XYZ,
  COLOR_SPACE_LAB,
  COLOR_SPACE_LUV,
  COLOR_SPACE_YCBCR,
  COLOR_SPACE_CMYK,
  COLOR_SPACE_YCCK,
  COLOR_SPACE_MAX
};

extern uns color_space_channels[COLOR_SPACE_MAX];
extern byte *color_space_name[COLOR_SPACE_MAX];

/* Color space ID <-> name conversions */
byte *color_space_id_to_name(uns id);
uns color_space_name_to_id(byte *name);

/* Struct color manipulation */
int color_get(struct color *color, byte *src, uns src_space);
int color_put(struct image_context *ctx, struct color *color, byte *dest, uns dest_space);

static inline void color_make_gray(struct color *color, uns gray)
{
  color->c[0] = gray;
  color->color_space = COLOR_SPACE_GRAYSCALE;
}

static inline void color_make_rgb(struct color *color, uns r, uns g, uns b)
{
  color->c[0] = r;
  color->c[1] = g;
  color->c[2] = b;
  color->color_space = COLOR_SPACE_RGB;
}

extern struct color color_black, color_white;

/* Conversion between various pixel formats */

enum {
  IMAGE_CONV_FILL_ALPHA = 1,
  IMAGE_CONV_COPY_ALPHA = 2,
  IMAGE_CONV_APPLY_ALPHA = 4,
};

struct image_conv_options {
  uns flags;
  struct color background;
};

extern struct image_conv_options image_conv_defaults;

int image_conv(struct image_context *ctx, struct image *dest, struct image *src, struct image_conv_options *opt);

/* Color spaces in the CIE 1931 chromacity diagram */

struct color_space_chromacity_info {
  double prim1[2];
  double prim2[2];
  double prim3[2];
  double white[2];
};

struct color_space_gamma_info {
  double simple_gamma;
  double detailed_gamma;
  double offset;
  double transition;
  double slope;
};

struct color_space_info {
  byte *name;
  struct color_space_chromacity_info chromacity;
  struct color_space_gamma_info gamma;
};

extern const double
  color_illuminant_d50[2],
  color_illuminant_d65[2],
  color_illuminant_e[2];

extern const struct color_space_info
  color_adobe_rgb_info,		/* Adobe RGB (1998) */
  color_apple_rgb_info,		/* Apple RGB */
  color_cie_rgb_info,		/* CIE RGB */
  color_color_match_rgb_info,	/* ColorMatch RGB */
  color_srgb_info;		/* sRGB */

/* These routines do not check numeric errors! */
void color_compute_color_space_to_xyz_matrix(double matrix[9], const struct color_space_chromacity_info *space);
void color_compute_bradford_matrix(double matrix[9], const double src[2], const double dest[2]);
void color_compute_color_spaces_conversion_matrix(double matrix[9], const struct color_space_chromacity_info *src, const struct color_space_chromacity_info *dest);
void color_invert_matrix(double dest[9], double matrix[9]);

static inline uns rgb_to_gray_func(uns r, uns g, uns b)
{
  return (r * 19660 + g * 38666 + b * 7210) >> 16;
}

/* Exact slow conversion routines */
void srgb_to_xyz_exact(double dest[3], double src[3]);
void xyz_to_srgb_exact(double dest[3], double src[3]);
void xyz_to_luv_exact(double dest[3], double src[3]);
void luv_to_xyz_exact(double dest[3], double src[3]);
void rgb_to_cmyk_exact(double dest[4], double src[3]);
void cmyk_to_rgb_exact(double dest[3], double src[4]);

/* Reference white */
#define REF_WHITE_X 0.96422
#define REF_WHITE_Y 1.
#define REF_WHITE_Z 0.82521

/* sRGB -> XYZ matrix */
#define SRGB_XYZ_XR 0.412424
#define SRGB_XYZ_XG 0.357579
#define SRGB_XYZ_XB 0.180464
#define SRGB_XYZ_YR 0.212656
#define SRGB_XYZ_YG 0.715158
#define SRGB_XYZ_YB 0.072186
#define SRGB_XYZ_ZR 0.019332
#define SRGB_XYZ_ZG 0.119193
#define SRGB_XYZ_ZB 0.950444


/*********************** OPTIMIZED CONVERSION ROUTINES **********************/

/* sRGB -> Luv parameters */
#define SRGB_TO_LUV_TAB2_SIZE 9
#define SRGB_TO_LUV_TAB2_SCALE 11
#define SRGB_TO_LUV_TAB3_SIZE 8
#define SRGB_TO_LUV_TAB3_SCALE (39 - SRGB_TO_LUV_TAB2_SCALE - SRGB_TO_LUV_TAB3_SIZE)

extern u16 srgb_to_luv_tab1[256];
extern u16 srgb_to_luv_tab2[9 << SRGB_TO_LUV_TAB2_SIZE];
extern u32 srgb_to_luv_tab3[20 << SRGB_TO_LUV_TAB3_SIZE];

void srgb_to_luv_init(void);
void srgb_to_luv_pixels(byte *dest, byte *src, uns count);

/* L covers the interval [0..255]; u and v are centered to 128 and scaled by 1/4 in respect of L */
static inline void srgb_to_luv_pixel(byte *dest, byte *src)
{
  uns r = srgb_to_luv_tab1[src[0]];
  uns g = srgb_to_luv_tab1[src[1]];
  uns b = srgb_to_luv_tab1[src[2]];
  uns x =
    (uns)(4 * SRGB_XYZ_XR * 0xffff) * r +
    (uns)(4 * SRGB_XYZ_XG * 0xffff) * g +
    (uns)(4 * SRGB_XYZ_XB * 0xffff) * b;
  uns y =
    (uns)(9 * SRGB_XYZ_YR * 0xffff) * r +
    (uns)(9 * SRGB_XYZ_YG * 0xffff) * g +
    (uns)(9 * SRGB_XYZ_YB * 0xffff) * b;
  uns l = srgb_to_luv_tab2[y >> (28 - SRGB_TO_LUV_TAB2_SIZE)];
    dest[0] = l >> (SRGB_TO_LUV_TAB2_SCALE - 8);
  uns sum =
    (uns)((SRGB_XYZ_XR + 15 * SRGB_XYZ_YR + 3 * SRGB_XYZ_ZR) * 0x7fff) * r +
    (uns)((SRGB_XYZ_XG + 15 * SRGB_XYZ_YG + 3 * SRGB_XYZ_ZG) * 0x7fff) * g +
    (uns)((SRGB_XYZ_XB + 15 * SRGB_XYZ_YB + 3 * SRGB_XYZ_ZB) * 0x7fff) * b;
  uns s = srgb_to_luv_tab3[sum >> (27 - SRGB_TO_LUV_TAB3_SIZE)];
  int xs = ((u64)x * s) >> 32;
  int ys = ((u64)y * s) >> 32;
  int xw = ((4 * 13) << (SRGB_TO_LUV_TAB3_SCALE - 4)) *
    REF_WHITE_X / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z);
  int yw = ((9 * 13) << (SRGB_TO_LUV_TAB3_SCALE - 4)) *
    REF_WHITE_Y / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z);
  int u = (int)(l) * (xs - xw);
  int v = (int)(l) * (ys - yw);
  dest[1] = 128 + (u >> (SRGB_TO_LUV_TAB3_SCALE + SRGB_TO_LUV_TAB2_SCALE - 10));
  dest[2] = 128 + (v >> (SRGB_TO_LUV_TAB3_SCALE + SRGB_TO_LUV_TAB2_SCALE - 10));
}


/****************** GENERAL INTERPOLATION IN 3D GRID ********************/

#define COLOR_CONV_SIZE	5  /* 128K conversion grid size */
#define COLOR_CONV_OFS	3  /* 8K interpolation table size */

struct color_grid_node {
  byte val[4];
};

struct color_interpolation_node {
  u16 ofs[4];
  u16 mul[4];
};

extern struct color_grid_node *srgb_to_luv_grid;
extern struct color_interpolation_node *color_interpolation_table;

void color_conv_init(void);
void color_conv_pixels(byte *dest, byte *src, uns count, struct color_grid_node *grid);

#define COLOR_CONV_SCALE_CONST (((((1 << COLOR_CONV_SIZE) - 1) << 16) + (1 << (16 - COLOR_CONV_OFS))) / 255)

static inline void color_conv_pixel(byte *dest, byte *src, struct color_grid_node *grid)
{
  uns s0 = src[0] * COLOR_CONV_SCALE_CONST;
  uns s1 = src[1] * COLOR_CONV_SCALE_CONST;
  uns s2 = src[2] * COLOR_CONV_SCALE_CONST;
  struct color_grid_node *g0, *g1, *g2, *g3, *g = grid +
    ((s0 >> 16) + ((s1 >> 16) << COLOR_CONV_SIZE) + ((s2 >> 16) << (2 * COLOR_CONV_SIZE)));
  struct color_interpolation_node *n = color_interpolation_table +
    (((s0 & (0x10000 - (0x10000 >> COLOR_CONV_OFS))) >> (16 - COLOR_CONV_OFS)) +
    ((s1 & (0x10000 - (0x10000 >> COLOR_CONV_OFS))) >> (16 - 2 * COLOR_CONV_OFS)) +
    ((s2 & (0x10000 - (0x10000 >> COLOR_CONV_OFS))) >> (16 - 3 * COLOR_CONV_OFS)));
  g0 = g + n->ofs[0];
  g1 = g + n->ofs[1];
  g2 = g + n->ofs[2];
  g3 = g + n->ofs[3];
  dest[0] = (g0->val[0] * n->mul[0] + g1->val[0] * n->mul[1] +
             g2->val[0] * n->mul[2] + g3->val[0] * n->mul[3] + 128) >> 8;
  dest[1] = (g0->val[1] * n->mul[0] + g1->val[1] * n->mul[1] +
             g2->val[1] * n->mul[2] + g3->val[1] * n->mul[3] + 128) >> 8;
  dest[2] = (g0->val[2] * n->mul[0] + g1->val[2] * n->mul[1] +
             g2->val[2] * n->mul[2] + g3->val[2] * n->mul[3] + 128) >> 8;
}

#endif
