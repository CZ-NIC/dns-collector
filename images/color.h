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
 *	- http://www.tecgraf.puc-rio.br/~mgattass/color/ColorIndex.html
 *
 *	FIXME:
 *	- fix theoretical problems with rounding errors in srgb_to_luv_pixel()
 *	- SIMD should help to speed up conversion of large arrays
 *	- maybe try to generate a long switch in color_conv_pixel()
 *	  with optimized entries instead of access to interpolation table
 */

#ifndef _IMAGES_COLOR_H
#define _IMAGES_COLOR_H

/* Exact slow conversion routines */
void srgb_to_xyz_slow(double dest[3], double src[3]);
void xyz_to_luv_slow(double dest[3], double src[3]);

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

static inline void
srgb_to_luv_pixel(byte *dest, byte *src)
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

static inline void
color_conv_pixel(byte *dest, byte *src, struct color_grid_node *grid)
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
