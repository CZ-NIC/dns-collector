/*
 *	UCW Library -- Table printer types
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#include <ucw/lib.h>
#include <ucw/table-types.h>
#include <ucw/fastbuf.h>
#include <ucw/table.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

/** xt_size **/

static const char *unit_suffix[] = {
  [SIZE_UNIT_BYTE] = "",
  [SIZE_UNIT_KILOBYTE] = "KB",
  [SIZE_UNIT_MEGABYTE] = "MB",
  [SIZE_UNIT_GIGABYTE] = "GB",
  [SIZE_UNIT_TERABYTE] = "TB"
};

static u64 unit_div[] = {
  [SIZE_UNIT_BYTE] = 1LLU,
  [SIZE_UNIT_KILOBYTE] = 1024LLU,
  [SIZE_UNIT_MEGABYTE] = 1024LLU * 1024LLU,
  [SIZE_UNIT_GIGABYTE] = 1024LLU * 1024LLU * 1024LLU,
  [SIZE_UNIT_TERABYTE] = 1024LLU * 1024LLU * 1024LLU * 1024LLU
};

static const char *xt_size_format(void *src, u32 fmt, struct mempool *pool)
{
  u64 curr_val = *(u64*) src;

  if(fmt == XTYPE_FMT_RAW) {
    return mp_printf(pool, "%"PRIu64, curr_val);
  }

  uint out_type = SIZE_UNIT_BYTE;

  if(fmt == XTYPE_FMT_DEFAULT) {
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

static const char * xt_size_fmt_parse(const char *opt_str, u32 *dest, struct mempool *pool UNUSED)
{
  if(opt_str == NULL) {
    return "NULL is not supported as a column argument.";
  }

  if(strlen(opt_str) == 0 || strcasecmp(opt_str, "b") == 0 || strcasecmp(opt_str, "bytes") == 0) {
    *dest = SIZE_UNIT_BYTE | SIZE_UNITS_FIXED;
    return NULL;
  }

  for(uint i = SIZE_UNIT_BYTE; i <= SIZE_UNIT_TERABYTE; i++) {
    if(strcasecmp(opt_str, unit_suffix[i]) == 0) {
      *dest = i | SIZE_UNITS_FIXED;
      return NULL;
    }
  }

  return "Unknown option.";
}

static const char *xt_size_parse(const char *str, void *dest, struct mempool *pool UNUSED)
{
  errno = 0;
  char *units_start = NULL;
  u64 parsed = strtol(str, &units_start, 10);
  if(str == units_start) {
    return mp_printf(pool, "Invalid value of size: '%s'.", str);
  }
  if(*units_start == 0) {
    *(u64*) dest = (u64) parsed;
    return NULL;
  }

  if(errno == EINVAL || errno == ERANGE) {
    return mp_printf(pool, "Invalid value of size: '%s'.", str);
  }

  for(uint i = 0; i < ARRAY_SIZE(unit_suffix); i++) {
    if(strcmp(unit_suffix[i], units_start) == 0) {
      *(u64*) dest = parsed * unit_div[i];
      return NULL;
    }
  }

  return mp_printf(pool, "Invalid format of size: '%s'.", str);
}

TABLE_COL_BODY(size, u64)

const struct xtype xt_size = {
  .size = sizeof(u64),
  .name = "size",
  .parse = xt_size_parse,
  .format = xt_size_format,
  .parse_fmt = xt_size_fmt_parse
};

/** xt_timestamp **/

#define FORMAT_TIME_SIZE 20	// Minimum buffer size

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

static const char * xt_timestamp_fmt_parse(const char *opt_str, u32 *dest, struct mempool *pool)
{
  if(opt_str == NULL) {
    return "NULL is not supported as a column argument.";
  }

  if(strcasecmp(opt_str, "timestamp") == 0 || strcasecmp(opt_str, "epoch") == 0) {
    *dest = TIMESTAMP_EPOCH;
    return NULL;
  } else if(strcasecmp(opt_str, "datetime") == 0) {
    *dest = TIMESTAMP_DATETIME;
    return NULL;
  }

  return mp_printf(pool, "Invalid column format option: '%s'.", opt_str);
}

TABLE_COL_BODY(timestamp, u64)

const struct xtype xt_timestamp = {
  .size = sizeof(u64),
  .name = "timestamp",
  //.parse = xt_timestamp_parse,
  .format = xt_timestamp_format,
  .parse_fmt = xt_timestamp_fmt_parse
};
