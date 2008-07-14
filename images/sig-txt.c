/*
 *	Image Library -- Detection of textured images
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "images/images.h"
#include "images/signature.h"
#include "images/math.h"

#include <string.h>

#define MAX_CELLS_COLS 4
#define MAX_CELLS_ROWS 4

void
image_sig_detect_textured(struct image_sig_data *data)
{
  if (image_sig_textured_threshold <= 0)
    {
      DBG("Zero textured threshold.");
      return;
    }

  uns cols = data->cols;
  uns rows = data->rows;
  uns cell_cols = MIN((cols + 1) / 2, MAX_CELLS_COLS);
  uns cell_rows = MIN((rows + 1) / 2, MAX_CELLS_ROWS);
  uns cell_x[MAX_CELLS_COLS + 1];
  uns cell_y[MAX_CELLS_ROWS + 1];
  uns i, j;
  u32 cnt[IMAGE_REG_MAX];

  if (cell_cols * cell_rows < 4)
    {
      DBG("Image is not textured.");
      return;
    }

  DBG("Detecting textured image... cols=%u rows=%u cell_cols=%u cell_rows=%u", cols, rows, cell_cols, cell_rows);

  /* Compute cells boundaries */
  for (i = 1, j = 0; i < cell_cols; i++)
    cell_x[i] = fast_div_u32_u8(j += cols, cell_cols);
  cell_x[0] = 0;
  cell_x[cell_cols] = cols;
  for (i = 1, j = 0; i < cell_rows; i++)
    cell_y[i] = fast_div_u32_u8(j += rows, cell_rows);
  cell_y[0] = 0;
  cell_y[cell_rows] = rows;

  /* Preprocess blocks */
  for (uns i = 0; i < data->regions_count; i++)
    for (struct image_sig_block *block = data->regions[i].blocks; block; block = block->next)
      block->region = i;

  /* Process cells */
  double e = 0;
  for (uns j = 0; j < cell_rows; j++)
    for (uns i = 0; i < cell_cols; i++)
      {
	uns cell_area = 0;
        bzero(cnt, data->regions_count * sizeof(u32));
	struct image_sig_block *b1 = data->blocks + cell_x[i] + cell_y[j] * cols, *b2;
        for (uns y = cell_y[j]; y < cell_y[j + 1]; y++, b1 += cols)
	  {
	    b2 = b1;
            for (uns x = cell_x[i]; x < cell_x[i + 1]; x++, b2++)
	      {
	        cnt[b2->region]++;
		cell_area++;
	      }
	  }
	for (uns k = 0; k < data->regions_count; k++)
	  {
	    int a = data->blocks_count * cnt[k] - cell_area * data->regions[k].count;
	    e += (double)a * a / ((double)isqr(data->regions[k].count) * cell_area);
	  }
      }

  DBG("Coefficient=%g", (double)e / (data->regions_count * data->blocks_count));

  /* Threshold */
  if (e < image_sig_textured_threshold * data->regions_count * data->blocks_count)
    {
      data->flags |= IMAGE_SIG_TEXTURED;
      DBG("Image is textured.");
    }
  else
    DBG("Image is not textured.");
}
