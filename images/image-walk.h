/*
 *	Image Library -- Pixels iteration
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#if !defined(IMAGE_WALK_INLINE) && !defined(IMAGE_WALK_STATIC)
#  error Missing IMAGE_WALK_INLINE or IMAGE_WALK_STATIC
#endif

#if !defined(IMAGE_WALK_UNROLL)
#  define IMAGE_WALK_UNROLL 1
#elif IMAGE_WALK_UNROLL != 1 && IMAGE_WALK_UNROLL != 2 && IMAGE_WALK_UNROLL != 4
#  error IMAGE_WALK_UNROLL must be 1, 2 or 4
#endif

#ifndef IMAGE_WALK_PIXELS
#  define IMAGE_WALK_PIXELS (img->pixels)
#endif
#ifndef IMAGE_WALK_COLS
#  define IMAGE_WALK_COLS (img->cols)
#endif
#ifndef IMAGE_WALK_ROWS
#  define IMAGE_WALK_ROWS (img->rows)
#endif
#ifndef IMAGE_WALK_COL_STEP
#  define IMAGE_WALK_COL_STEP (img->pixel_size)
#endif
#ifndef IMAGE_WALK_ROW_STEP
#  define IMAGE_WALK_ROW_STEP (img->row_size)
#endif

#ifdef IMAGE_WALK_DOUBLE
#  ifndef IMAGE_WALK_SEC_PIXELS
#    define IMAGE_WALK_SEC_PIXELS (sec_img->pixels)
#  endif
#  ifndef IMAGE_WALK_SEC_COLS
#    define IMAGE_WALK_SEC_COLS (sec_img->cols)
#  endif
#  ifndef IMAGE_WALK_SEC_ROWS
#    define IMAGE_WALK_SEC_ROWS (sec_img->rows)
#  endif
#  ifndef IMAGE_WALK_SEC_COL_STEP
#    define IMAGE_WALK_SEC_COL_STEP (sec_img->pixel_size)
#  endif
#  ifndef IMAGE_WALK_SEC_ROW_STEP
#    define IMAGE_WALK_SEC_ROW_STEP (sec_img->row_size)
#  endif
#  define STEP IMAGE_WALK_DO_STEP; pos += col_step; sec_pos += sec_col_step
#else
#  define STEP IMAGE_WALK_DO_STEP; pos += col_step
#endif

#ifndef IMAGE_WALK_DO_START
#  define IMAGE_WALK_DO_START
#endif

#ifndef IMAGE_WALK_DO_END
#  define IMAGE_WALK_DO_END
#endif

#ifndef IMAGE_WALK_DO_ROW_START
#  define IMAGE_WALK_DO_ROW_START
#endif

#ifndef IMAGE_WALK_DO_ROW_END
#  define IMAGE_WALK_DO_ROW_END
#endif

#ifndef IMAGE_WALK_DO_STEP
#  define IMAGE_WALK_DO_STEP
#endif

#ifndef IMAGE_WALK_INLINE
static void IMAGE_WALK_STATIC
    (struct image *img
#   ifdef IMAGE_WALK_DOUBLE
    , struct image *sec_img
#   endif
#   ifdef IMAGE_WALK_EXTRA_ARGS
    , IMAGE_WALK_EXTRA_ARGS
#   endif
    )
#endif
{
  uns cols = IMAGE_WALK_COLS;
  uns rows = IMAGE_WALK_ROWS;
# if IMAGE_WALK_UNROLL > 1
  uns cols_unroll_block_count = cols / IMAGE_WALK_UNROLL;
  uns cols_unroll_end_count = cols % IMAGE_WALK_UNROLL;
# endif
  byte *pos = IMAGE_WALK_PIXELS, *row_start = pos;
  int col_step = IMAGE_WALK_COL_STEP;
  int row_step = IMAGE_WALK_ROW_STEP;
# ifdef IMAGE_WALK_DOUBLE
  byte *sec_pos = IMAGE_WALK_SEC_PIXELS, *sec_row_start = sec_pos;
  int sec_col_step = IMAGE_WALK_SEC_COL_STEP;
  int sec_row_step = IMAGE_WALK_SEC_ROW_STEP;
# endif
  IMAGE_WALK_DO_START;
  while (rows--)
    {
      IMAGE_WALK_DO_ROW_START;
#     if IMAGE_WALK_UNROLL == 1
      for (uns i = cols; i--; )
#     else
      for (uns i = cols_unroll_block_count; i--; )
#     endif
        {
#         if IMAGE_WALK_UNROLL >= 4
	  STEP;
	  STEP;
#         endif
#         if IMAGE_WALK_UNROLL >= 2
	  STEP;
#         endif
	  STEP;
	}
#     if IMAGE_WALK_UNROLL > 1
      for (uns i = cols_unroll_end_count; i--; )
        {
	  STEP;
	}
#     endif
      IMAGE_WALK_DO_ROW_END;
      pos = (row_start += row_step);
#     ifdef IMAGE_WALK_DOUBLE
      sec_pos = (sec_row_start += sec_row_step);
#     endif
    }
  IMAGE_WALK_DO_END;
}

#undef IMAGE_WALK_INLINE
#undef IMAGE_WALK_STATIC
#undef IMAGE_WALK_UNROLL
#undef IMAGE_WALK_DOUBLE
#undef IMAGE_WALK_EXTRA_ARGS
#undef IMAGE_WALK_PIXELS
#undef IMAGE_WALK_COLS
#undef IMAGE_WALK_ROWS
#undef IMAGE_WALK_COL_STEP
#undef IMAGE_WALK_ROW_STEP
#undef IMAGE_WALK_SEC_PIXELS
#undef IMAGE_WALK_SEC_COLS
#undef IMAGE_WALK_SEC_ROWS
#undef IMAGE_WALK_SEC_COL_STEP
#undef IMAGE_WALK_SEC_ROW_STEP
#undef IMAGE_WALK_DO_START
#undef IMAGE_WALK_DO_END
#undef IMAGE_WALK_DO_ROW_START
#undef IMAGE_WALK_DO_ROW_END
#undef IMAGE_WALK_DO_STEP
#undef STEP
