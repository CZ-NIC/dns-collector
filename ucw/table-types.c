#include <ucw/lib.h>
#include <ucw/table-types.h>
#include <ucw/fastbuf.h>
#include <ucw/table.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

const struct xtype xt_size;
const struct xtype xt_timestamp;

static const char *unit_suffix[] = {
  [SIZE_UNIT_BYTE] = "",
  [SIZE_UNIT_KILOBYTE] = "KB",
  [SIZE_UNIT_MEGABYTE] = "MB",
  [SIZE_UNIT_GIGABYTE] = "GB",
  [SIZE_UNIT_TERABYTE] = "TB"
};

bool table_set_col_opt_size(struct table *tbl, uint col_inst_idx, const char *col_arg, char **err)
{
  struct table_column *col_def = tbl->column_order[col_inst_idx].col_def;
  if(col_def->type_def != COL_TYPE_SIZE) {
    *err = NULL;
    return false;
  }

  if(col_arg == NULL || strcasecmp(col_arg, "b") == 0 || strcasecmp(col_arg, "bytes") == 0) {
    tbl->column_order[col_inst_idx].output_type = SIZE_UNIT_BYTE | SIZE_UNITS_FIXED;
    *err = NULL;
    return true;
  }

  tbl->column_order[col_inst_idx].output_type = XTYPE_FMT_DEFAULT; // CELL_OUT_UNINITIALIZED;
  for(uint i = SIZE_UNIT_BYTE; i <= SIZE_UNIT_TERABYTE; i++) {
    if(strcasecmp(col_arg, unit_suffix[i]) == 0) {
      tbl->column_order[col_inst_idx].output_type = i | SIZE_UNITS_FIXED;
    }
  }

  if(tbl->column_order[col_inst_idx].output_type == XTYPE_FMT_DEFAULT) {
    *err = mp_printf(tbl->pool, "Invalid column format option: '%s' for column %d (counted from 0)", col_arg, col_inst_idx);
    return true;
  }

  *err = NULL;
  return true;
}

bool table_set_col_opt_timestamp(struct table *tbl, uint col_inst_idx, const char *col_arg, char **err)
{
  int col_type_idx = tbl->column_order[col_inst_idx].idx;
  if(tbl->columns[col_type_idx].type_def != COL_TYPE_TIMESTAMP) {
    *err = NULL;
    return false;
  }

  if(col_arg == NULL) {
    *err = NULL;
    return true;
  }

  if(strcasecmp(col_arg, "timestamp") == 0 || strcasecmp(col_arg, "epoch") == 0) {
    tbl->column_order[col_inst_idx].output_type = TIMESTAMP_EPOCH;
  } else if(strcasecmp(col_arg, "datetime") == 0) {
    tbl->column_order[col_inst_idx].output_type = TIMESTAMP_DATETIME;
  } else {
    *err = mp_printf(tbl->pool, "Invalid column format option: '%s' for column %d.", col_arg, col_inst_idx);
    return true;
  }

  *err = NULL;
  return true;
}

void table_col_size_name(struct table *tbl, const char *col_name, u64 val)
{
  int col = table_get_col_idx(tbl, col_name);
  table_col_size(tbl, col, val);
}

void table_col_size(struct table *tbl, int col, u64 val)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(tbl->columns[col].type_def == COL_TYPE_ANY || COL_TYPE_SIZE == tbl->columns[col].type_def);

  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;

  static u64 unit_div[] = {
    [SIZE_UNIT_BYTE] = (u64) 1,
    [SIZE_UNIT_KILOBYTE] = (u64) 1024LLU,
    [SIZE_UNIT_MEGABYTE] = (u64) (1024LLU * 1024LLU),
    [SIZE_UNIT_GIGABYTE] = (u64) (1024LLU * 1024LLU * 1024LLU),
    [SIZE_UNIT_TERABYTE] = (u64) (1024LLU * 1024LLU * 1024LLU * 1024LLU)
  };

  // FIXME: add the SIZE_UNIT_AUTO
  TBL_COL_ITER_START(tbl, col, curr_col, curr_col_idx) {
    // FIXME: do some rounding? Or maybe use double and floating-point printing?
    uint out_type = 0;
    u64 curr_val = val;

    if(curr_col->output_type == XTYPE_FMT_DEFAULT || curr_col->output_type == XTYPE_FMT_RAW) {
      curr_val = curr_val / unit_div[SIZE_UNIT_BYTE];
      out_type = SIZE_UNIT_BYTE;
    } else if(curr_col->output_type == XTYPE_FMT_PRETTY) {
      curr_val = curr_val / unit_div[SIZE_UNIT_BYTE];
      out_type = SIZE_UNIT_BYTE; // curr_col->output_type;
    } else if((curr_col->output_type & SIZE_UNITS_FIXED) != 0) {
      curr_val = curr_val / unit_div[curr_col->output_type & ~SIZE_UNITS_FIXED];
      out_type = curr_col->output_type & ~SIZE_UNITS_FIXED;
    }

    curr_col->cell_content = mp_printf(tbl->pool, "%lu%s", curr_val, unit_suffix[out_type]);
  } TBL_COL_ITER_END
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
  ASSERT(tbl->columns[col].type_def == COL_TYPE_ANY || COL_TYPE_TIMESTAMP == tbl->columns[col].type_def);

  char formatted_time_buf[FORMAT_TIME_SIZE] = { 0 };

  time_t tmp_time = (time_t)val;
  struct tm t = *gmtime(&tmp_time);
  TBL_COL_ITER_START(tbl, col, curr_col, curr_col_idx) {
    switch (curr_col->output_type) {
    case XTYPE_FMT_DEFAULT:
    case XTYPE_FMT_RAW:
      sprintf(formatted_time_buf, "%lu", val);
      break;
    case XTYPE_FMT_PRETTY:
      strftime(formatted_time_buf, FORMAT_TIME_SIZE, "%F %T", &t);
    break;
    default:
      abort();
      break;
    }

    curr_col->cell_content = mp_printf(tbl->pool, "%s", formatted_time_buf);
  } TBL_COL_ITER_END
}
