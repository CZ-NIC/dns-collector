#include <ucw/lib.h>
#include <ucw/config.h>
#include <ucw/table-types.h>
#include <ucw/fastbuf.h>
#include <ucw/config.h>
#include <ucw/table.h>
#include <time.h>
#include <stdio.h>

void table_col_name(struct table *tbl, const char *col_name, u64 val)
{
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
  val = val / unit_div[tbl->column_order[col].output_type];

  tbl->col_str_ptrs[col] = mp_printf(tbl->pool, "%lu%s", val, unit_suffix[tbl->column_order[col].output_type]);
}

#define FORMAT_TIME_SIZE 20	// Minimum buffer size

void table_col_timestamp(struct table *tbl, int col, u64 val)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(tbl->columns[col].type == COL_TYPE_ANY || COL_TYPE_TIMESTAMP == tbl->columns[col].type);
  //ASSERT(fmt != NULL);

  char formatted_time_buf[FORMAT_TIME_SIZE];

  time_t tmp_time = (time_t)val;
  struct tm t = *gmtime(&tmp_time);

  switch (tbl->column_order[col].output_type) {
  case TIMESTAMP_EPOCH:
    sprintf(formatted_time_buf, "%u", (uint) val);
    break;
  case TIMESTAMP_DATETIME:
    strftime(formatted_time_buf, FORMAT_TIME_SIZE, "%F %T", &t);
  default:
    break;
  }

  tbl->col_str_ptrs[col] = mp_printf(tbl->pool, "%s", formatted_time_buf);
}

