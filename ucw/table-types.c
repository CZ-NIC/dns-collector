#include <ucw/lib.h>
#include <ucw/table-types.h>
#include <ucw/fastbuf.h>
#include <ucw/table.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/** xt_size **/

static const char *unit_suffix[] = {
  [SIZE_UNIT_BYTE] = "",
  [SIZE_UNIT_KILOBYTE] = "KB",
  [SIZE_UNIT_MEGABYTE] = "MB",
  [SIZE_UNIT_GIGABYTE] = "GB",
  [SIZE_UNIT_TERABYTE] = "TB"
};

static const char *xt_size_format(void *src, u32 fmt, struct mempool *pool)
{
  u64 curr_val = *(u64*) src;
  uint out_type = SIZE_UNIT_BYTE;

  static u64 unit_div[] = {
    [SIZE_UNIT_BYTE] = 1LLU,
    [SIZE_UNIT_KILOBYTE] = 1024LLU,
    [SIZE_UNIT_MEGABYTE] = 1024LLU * 1024LLU,
    [SIZE_UNIT_GIGABYTE] = 1024LLU * 1024LLU * 1024LLU,
    [SIZE_UNIT_TERABYTE] = 1024LLU * 1024LLU * 1024LLU * 1024LLU
  };

  if(fmt == XTYPE_FMT_DEFAULT || fmt == XTYPE_FMT_RAW) {
    curr_val = curr_val / unit_div[SIZE_UNIT_BYTE];
    out_type = SIZE_UNIT_BYTE;
  } else if(fmt == XTYPE_FMT_PRETTY) {
    curr_val = curr_val / unit_div[SIZE_UNIT_BYTE];
    out_type = SIZE_UNIT_BYTE;
  } else if((fmt & SIZE_UNITS_FIXED) != 0) {
    curr_val = curr_val / unit_div[fmt & ~SIZE_UNITS_FIXED];
    out_type = fmt & ~SIZE_UNITS_FIXED;
  }

  return mp_printf(pool, "%"PRIu64"%s", curr_val, unit_suffix[out_type]);
}

bool table_set_col_opt_size(struct table *tbl, uint col_inst_idx, const char *col_arg, char **err)
{
  const struct table_column *col_def = tbl->column_order[col_inst_idx].col_def;
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

TABLE_COL_BODY(size, u64)

const struct xtype xt_size = {
  .size = sizeof(u64),
  .name = "size",
  //.parse = xt_size_parse,
  .format = xt_size_format,
};

/** xt_timestamp **/

#define FORMAT_TIME_SIZE 20	// Minimum buffer size

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

static const char *xt_timestamp_format(void *src, u32 fmt, struct mempool *pool)
{
  char formatted_time_buf[FORMAT_TIME_SIZE] = { 0 };

  u64 tmp_time_u64 = *(u64*)src;
  time_t tmp_time = (time_t) tmp_time_u64;
  struct tm t = *gmtime(&tmp_time);
  switch (fmt) {
  case XTYPE_FMT_DEFAULT:
  case XTYPE_FMT_RAW:
    sprintf(formatted_time_buf, "%"PRIu64, tmp_time_u64);
    break;
  case XTYPE_FMT_PRETTY:
    strftime(formatted_time_buf, FORMAT_TIME_SIZE, "%F %T", &t);
    break;
  default:
    ASSERT(0);
    break;
  }

  return mp_printf(pool, "%s", formatted_time_buf);
}

TABLE_COL_BODY(timestamp, u64)

const struct xtype xt_timestamp = {
  .size = sizeof(u64),
  .name = "timestamp",
  //.parse = xt_timestamp_parse,
  .format = xt_timestamp_format,
};
