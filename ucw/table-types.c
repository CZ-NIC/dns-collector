#include <ucw/lib.h>
#include <ucw/config.h>
#include <ucw/table-types.h>
#include <ucw/fastbuf.h>
#include <ucw/config.h>
#include <ucw/table.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void table_col_size_name(struct table *tbl, const char *col_name, u64 val)
{
  int col = table_get_col_idx(tbl, col_name);
  table_col_size(tbl, col, val);
}

void table_col_size(struct table *tbl, int col, u64 val)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(tbl->columns[col].type == COL_TYPE_ANY || COL_TYPE_SIZE == tbl->columns[col].type);

  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;

  static u64 unit_div[] = {
    [UNIT_BYTE] = (u64) 1,
    [UNIT_KILOBYTE] = (u64) 1024LU,
    [UNIT_MEGABYTE] = (u64) (1024LU * 1024LU),
    [UNIT_GIGABYTE] = (u64) (1024LU * 1024LU * 1024LU),
    [UNIT_TERABYTE] = (u64) (1024LU * 1024LU * 1024LU * 1024LU)
  };

  static const char *unit_suffix[] = {
    [UNIT_BYTE] = "",
    [UNIT_KILOBYTE] = "KB",
    [UNIT_MEGABYTE] = "MB",
    [UNIT_GIGABYTE] = "GB",
    [UNIT_TERABYTE] = "TB"
  };

  // FIXME: do some rounding?
  uint out_type = 0;
  if(tbl->column_order[col].output_type == CELL_OUT_UNINITIALIZED) {
    val = val / unit_div[UNIT_BYTE];
    out_type = 0;
  } else {
    val = val / unit_div[tbl->column_order[col].output_type];
    out_type = tbl->column_order[col].output_type;
  }

  table_col_printf(tbl, col, "%lu%s", val, unit_suffix[out_type]);
}

#define FORMAT_TIME_SIZE 20	// Minimum buffer size

void table_col_timestamp_name(struct table *tbl, const char * col_name, u64 val)
{
  int col = table_get_col_idx(tbl, col_name);
  table_col_size(tbl, col, val);
}

void table_col_timestamp(struct table *tbl, int col, u64 val)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(tbl->columns[col].type == COL_TYPE_ANY || COL_TYPE_TIMESTAMP == tbl->columns[col].type);

  char formatted_time_buf[FORMAT_TIME_SIZE] = { 0 };

  time_t tmp_time = (time_t)val;
  struct tm t = *gmtime(&tmp_time);

  switch (tbl->column_order[col].output_type) {
  case TIMESTAMP_EPOCH:
  case CELL_OUT_UNINITIALIZED:
    sprintf(formatted_time_buf, "%lu", val);
    break;
  case TIMESTAMP_DATETIME:
    strftime(formatted_time_buf, FORMAT_TIME_SIZE, "%F %T", &t);
    break;
  default:
    abort();
    break;
  }

  table_col_printf(tbl, col, "%s", formatted_time_buf);
}

