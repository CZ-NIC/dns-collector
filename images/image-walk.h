/*
 *	Image Library -- Pixels iteration
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef IMAGE_WALK_PREFIX
#  error Undefined IMAGE_WALK_PREFIX
#endif

#define P(x) IMAGE_WALK_PREFIX(x)

#if !defined(IMAGE_WALK_UNROLL)
#  define IMAGE_WALK_UNROLL 1
#elif IMAGE_WALK_UNROLL != 1 && IMAGE_WALK_UNROLL != 2 && IMAGE_WALK_UNROLL != 4
#  error IMAGE_WALK_UNROLL must be 1, 2 or 4
#endif

#ifndef IMAGE_WALK_IMAGE
#  define IMAGE_WALK_IMAGE P(img)
#endif
#ifndef IMAGE_WALK_PIXELS
#  define IMAGE_WALK_PIXELS (IMAGE_WALK_IMAGE->pixels)
#endif
#ifndef IMAGE_WALK_COLS
#  define IMAGE_WALK_COLS (IMAGE_WALK_IMAGE->cols)
#endif
#ifndef IMAGE_WALK_ROWS
#  define IMAGE_WALK_ROWS (IMAGE_WALK_IMAGE->rows)
#endif
#ifndef IMAGE_WALK_COL_STEP
#  define IMAGE_WALK_COL_STEP (IMAGE_WALK_IMAGE->pixel_size)
#endif
#ifndef IMAGE_WALK_ROW_STEP
#  define IMAGE_WALK_ROW_STEP (IMAGE_WALK_IMAGE->row_size)
#endif

#ifdef IMAGE_WALK_DOUBLE
#  ifndef IMAGE_WALK_SEC_IMAGE
#    define IMAGE_WALK_SEC_IMAGE P(sec_img)
#  endif
#  ifndef IMAGE_WALK_SEC_PIXELS
#    define IMAGE_WALK_SEC_PIXELS (IMAGE_WALK_SEC_IMAGE->pixels)
#  endif
#  ifndef IMAGE_WALK_SEC_COLS
#    define IMAGE_WALK_SEC_COLS (IMAGE_WALK_SEC_IMAGE->cols)
#  endif
#  ifndef IMAGE_WALK_SEC_ROWS
#    define IMAGE_WALK_SEC_ROWS (IMAGE_WALK_SEC_IMAGE->rows)
#  endif
#  ifndef IMAGE_WALK_SEC_COL_STEP
#    define IMAGE_WALK_SEC_COL_STEP (IMAGE_WALK_SEC_IMAGE->pixel_size)
#  endif
#  ifndef IMAGE_WALK_SEC_ROW_STEP
#    define IMAGE_WALK_SEC_ROW_STEP (IMAGE_WALK_SEC_IMAGE->row_size)
#  endif
#  define IMAGE_WALK__STEP IMAGE_WALK_DO_STEP; P(pos) += P(col_step); P(sec_pos) += P(sec_col_step)
#else
#  define IMAGE_WALK__STEP IMAGE_WALK_DO_STEP; P(pos) += P(col_step)
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
static void P(walk)
    (struct image *P(img)
#   ifdef IMAGE_WALK_DOUBLE
    , struct image *P(sec_img)
#   endif
#   ifdef IMAGE_WALK_EXTRA_ARGS
    , IMAGE_WALK_EXTRA_ARGS
#   endif
    )
#endif
{
  uns P(cols) = IMAGE_WALK_COLS;
  uns P(rows) = IMAGE_WALK_ROWS;
# if IMAGE_WALK_UNROLL > 1
  uns P(cols_unroll_block_count) = P(cols) / IMAGE_WALK_UNROLL;
  uns P(cols_unroll_end_count) = P(cols) % IMAGE_WALK_UNROLL;
# endif
  byte *P(pos) = IMAGE_WALK_PIXELS, *P(row_start) = P(pos);
  int P(col_step) = IMAGE_WALK_COL_STEP;
  int P(row_step) = IMAGE_WALK_ROW_STEP;
# ifdef IMAGE_WALK_DOUBLE
  byte *P(sec_pos) = IMAGE_WALK_SEC_PIXELS, *P(sec_row_start) = P(sec_pos);
  int P(sec_col_step) = IMAGE_WALK_SEC_COL_STEP;
  int P(sec_row_step) = IMAGE_WALK_SEC_ROW_STEP;
# endif
  IMAGE_WALK_DO_START;
  while (P(rows)--)
    {
      IMAGE_WALK_DO_ROW_START;
#     if IMAGE_WALK_UNROLL == 1
      for (uns P(_i) = P(cols); P(_i)--; )
#     else
      for (uns P(_i) = P(cols_unroll_block_count); P(_i)--; )
#     endif
        {
#         if IMAGE_WALK_UNROLL >= 4
	  IMAGE_WALK__STEP;
	  IMAGE_WALK__STEP;
#         endif
#         if IMAGE_WALK_UNROLL >= 2
	  IMAGE_WALK__STEP;
#         endif
	  IMAGE_WALK__STEP;
	}
#     if IMAGE_WALK_UNROLL > 1
      for (uns P(_i) = P(cols_unroll_end_count); P(_i)--; )
        {
	  IMAGE_WALK__STEP;
	}
#     endif
      IMAGE_WALK_DO_ROW_END;
      P(pos) = (P(row_start) += P(row_step));
#     ifdef IMAGE_WALK_DOUBLE
      P(sec_pos) = (P(sec_row_start) += P(sec_row_step));
#     endif
    }
  IMAGE_WALK_DO_END;
}

#undef IMAGE_WALK_PREFIX
#undef IMAGE_WALK_INLINE
#undef IMAGE_WALK_UNROLL
#undef IMAGE_WALK_DOUBLE
#undef IMAGE_WALK_EXTRA_ARGS
#undef IMAGE_WALK_IMAGE
#undef IMAGE_WALK_PIXELS
#undef IMAGE_WALK_COLS
#undef IMAGE_WALK_ROWS
#undef IMAGE_WALK_COL_STEP
#undef IMAGE_WALK_ROW_STEP
#undef IMAGE_WALK_SEC_IMAGE
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
#undef IMAGE_WALK__STEP
#undef P
