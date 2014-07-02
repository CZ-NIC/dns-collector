#include <ucw/lib.h>
#include <ucw/config.h>
#include <ucw/table-types.h>
#include <ucw/fastbuf.h>
#include <ucw/config.h>
#include <ucw/table.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

static bool table_set_col_opt_size(struct table *tbl, uint col_copy_idx, const char *col_arg, char **err)
{
  int col_type_idx = tbl->column_order[col_copy_idx].idx;
  if(tbl->columns[col_type_idx].type == COL_TYPE_SIZE) {
    if(strcasecmp(col_arg, "b") == 0 || strcasecmp(col_arg, "bytes") == 0) {
      tbl->column_order[col_copy_idx].output_type = UNIT_BYTE;
    } else if(strcasecmp(col_arg, "kb") == 0) {
      tbl->column_order[col_copy_idx].output_type = UNIT_KILOBYTE;
    } else if(strcasecmp(col_arg, "mb") == 0) {
      tbl->column_order[col_copy_idx].output_type = UNIT_MEGABYTE;
    } else if(strcasecmp(col_arg, "gb") == 0) {
      tbl->column_order[col_copy_idx].output_type = UNIT_GIGABYTE;
    } else if(strcasecmp(col_arg, "tb") == 0) {
      tbl->column_order[col_copy_idx].output_type = UNIT_TERABYTE;
    } else {
      *err = mp_printf(tbl->pool, "Tableprinter: invalid column format option: '%s' for column %d (counted from 0)", col_arg, col_copy_idx);
      return true;
    }
    *err = NULL;
    return true;
  }

  *err = NULL;
  return false;
}

struct table_user_type table_type_size = {
  .set_col_instance_option = table_set_col_opt_size,
  .type = COL_TYPE_SIZE,
};

static bool table_set_col_opt_timestamp(struct table *tbl, uint col_copy_idx, const char *col_arg, char **err)
{
  int col_type_idx = tbl->column_order[col_copy_idx].idx;
  if(tbl->columns[col_type_idx].type == COL_TYPE_TIMESTAMP) {
    if(strcasecmp(col_arg, "timestamp") == 0 || strcasecmp(col_arg, "epoch") == 0) {
      tbl->column_order[col_copy_idx].output_type = TIMESTAMP_EPOCH;
    } else if(strcasecmp(col_arg, "datetime") == 0) {
      tbl->column_order[col_copy_idx].output_type = TIMESTAMP_DATETIME;
    } else {
      *err = mp_printf(tbl->pool, "Tableprinter: invalid column format option: '%s' for column %d.", col_arg, col_copy_idx);
      return true;
    }
    *err = NULL;
    return true;
  }

  *err = NULL;
  return false;
}

struct table_user_type table_type_timestamp = {
  .set_col_instance_option = table_set_col_opt_timestamp,
  .type = COL_TYPE_TIMESTAMP,
};

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
    [UNIT_KILOBYTE] = (u64) 1024LLU,
    [UNIT_MEGABYTE] = (u64) (1024LLU * 1024LLU),
    [UNIT_GIGABYTE] = (u64) (1024LLU * 1024LLU * 1024LLU),
    [UNIT_TERABYTE] = (u64) (1024LLU * 1024LLU * 1024LLU * 1024LLU)
  };

  static const char *unit_suffix[] = {
    [UNIT_BYTE] = "",
    [UNIT_KILOBYTE] = "KB",
    [UNIT_MEGABYTE] = "MB",
    [UNIT_GIGABYTE] = "GB",
    [UNIT_TERABYTE] = "TB"
  };

  int curr_col = tbl->columns[col].first_column;
  while(curr_col != -1) {

    // FIXME: do some rounding?
    uint out_type = 0;
    if(tbl->column_order[curr_col].output_type == CELL_OUT_UNINITIALIZED) {
      val = val / unit_div[UNIT_BYTE];
      out_type = 0;
    } else {
      val = val / unit_div[tbl->column_order[curr_col].output_type];
      out_type = tbl->column_order[curr_col].output_type;
    }

    tbl->column_order[curr_col].cell_content = mp_printf(tbl->pool, "%lu%s", val, unit_suffix[out_type]);
    curr_col = tbl->column_order[curr_col].next_column;
  }

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

  int curr_col = tbl->columns[col].first_column;
  while(curr_col != -1) {
    switch (tbl->column_order[curr_col].output_type) {
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

    tbl->column_order[curr_col].cell_content = mp_printf(tbl->pool, "%s", formatted_time_buf);
    curr_col = tbl->column_order[curr_col].next_column;
  }
}
