/*
 *	UCW Library -- Table printer types
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#include <ucw/lib.h>
#include <ucw/table-types.h>
#include <ucw/fastbuf.h>
#include <ucw/strtonum.h>
#include <ucw/table.h>
#include <time.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

// FIXME: I seriously doubt there is any good reason for keeping
// these types separated from the generic xtype machinery. There
// is nothing special in them, which would be tightly connected
// to the table printer. Especially as they are already tested
// by xtypes-test.c.  --mj

/** xt_size **/

static const struct unit_definition xt_size_units[] = {
  [XT_SIZE_UNIT_BYTE] = { "", 1LLU, 1 },
  [XT_SIZE_UNIT_KILOBYTE] = { "KB", 1024LLU, 1 },
  [XT_SIZE_UNIT_MEGABYTE] = { "MB", 1024LLU * 1024LLU, 1 },
  [XT_SIZE_UNIT_GIGABYTE] = { "GB", 1024LLU * 1024LLU * 1024LLU, 1 },
  [XT_SIZE_UNIT_TERABYTE] = { "TB", 1024LLU * 1024LLU * 1024LLU * 1024LLU, 1 },
  { 0, 0, 0 }
};

static enum size_units xt_size_auto_units(u64 sz)
{
  if(sz >= xt_size_units[XT_SIZE_UNIT_TERABYTE].num) {
    return XT_SIZE_UNIT_TERABYTE;
  } else if(sz >= xt_size_units[XT_SIZE_UNIT_GIGABYTE].num) {
    return XT_SIZE_UNIT_GIGABYTE;
  } else if(sz >= xt_size_units[XT_SIZE_UNIT_MEGABYTE].num) {
    return XT_SIZE_UNIT_MEGABYTE;
  } else if(sz >= xt_size_units[XT_SIZE_UNIT_KILOBYTE].num) {
    return XT_SIZE_UNIT_KILOBYTE;
  }

  return XT_SIZE_UNIT_BYTE;
}

static const char *xt_size_format(void *src, u32 fmt, struct mempool *pool)
{
  u64 curr_val = *(u64*) src;
  uint out_units;

  if(fmt & XT_SIZE_FMT_FIXED_UNIT) {
    out_units = fmt & ~XT_SIZE_FMT_FIXED_UNIT;
  } else {
    switch(fmt) {
    case XTYPE_FMT_RAW:
      return mp_printf(pool, "%"PRIu64, curr_val);
    case XTYPE_FMT_PRETTY:
      out_units = XT_SIZE_UNIT_AUTO;
      break;
    case XTYPE_FMT_DEFAULT:
    default:
      out_units = XT_SIZE_UNIT_BYTE;
      break;
    }
  }

  if(out_units == XT_SIZE_UNIT_AUTO) {
    out_units = xt_size_auto_units(curr_val);
  }
  ASSERT(out_units < ARRAY_SIZE(xt_size_units));

  curr_val = curr_val / xt_size_units[out_units].num;
  return mp_printf(pool, "%"PRIu64"%s", curr_val, xt_size_units[out_units].unit);
}

static const char *xt_size_fmt_parse(const char *opt_str, u32 *dest, struct mempool *pool)
{
  if(strlen(opt_str) == 0 || strcmp(opt_str, "B") == 0 || strcmp(opt_str, "Bytes") == 0) {
    *dest = XT_SIZE_FMT_UNIT(XT_SIZE_UNIT_BYTE);
    return NULL;
  }

  if(strcmp(opt_str, "auto") == 0) {
    *dest = XT_SIZE_FMT_UNIT(XT_SIZE_UNIT_AUTO);
    return NULL;
  }

  int unit_idx = xtype_unit_parser(opt_str, xt_size_units);
  if(unit_idx == -1) {
    return mp_printf(pool, "Unknown option '%s'", opt_str);
  }

  *dest = XT_SIZE_FMT_UNIT(unit_idx);
  return NULL;
}

static const char *xt_size_parse(const char *str, void *dest, struct mempool *pool)
{
  errno = 0;
  const char *units_start = NULL;
  u64 parsed;
  const char *err = str_to_u64(&parsed, str, &units_start, 10 | STN_FLAGS);
  if(err != NULL) {
    return mp_printf(pool, "Invalid value of size: '%s'; number parser error: %s.", str, err);
  }

  if(*units_start == 0) {
    *(u64*) dest = (u64) parsed;
    return NULL;
  }

  int unit_idx = xtype_unit_parser(units_start, xt_size_units);
  if(unit_idx == -1) {
    return mp_printf(pool, "Invalid units: '%s'.", str);
  }

  // FIXME: Detect overflow?
  u64 num = xt_size_units[unit_idx].num;
  if((parsed && UINT64_MAX / parsed < num) ||
     (num && UINT64_MAX / num < parsed)) {
    return mp_printf(pool, "Size too large: '%s'.", str);
  }

  *(u64*) dest = parsed * xt_size_units[unit_idx].num;
  return NULL;
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

  return mp_strdup(pool, formatted_time_buf);
}

static const char *xt_timestamp_fmt_parse(const char *opt_str, u32 *dest, struct mempool *pool)
{
  if(strcasecmp(opt_str, "timestamp") == 0 || strcasecmp(opt_str, "epoch") == 0) {
    *dest = XT_TIMESTAMP_FMT_EPOCH;
    return NULL;
  } else if(strcasecmp(opt_str, "datetime") == 0) {
    *dest = XT_TIMESTAMP_FMT_DATETIME;
    return NULL;
  }

  return mp_printf(pool, "Invalid column format option: '%s'.", opt_str);
}

static const char *xt_timestamp_parse(const char *str, void *dest, struct mempool *pool)
{
  errno = 0;
  const char *parse_end = NULL;
  u64 parsed;
  const char *err = str_to_u64(&parsed, str, &parse_end, 10 | STN_FLAGS);
  if(str == parse_end) {
    return mp_printf(pool, "Invalid value of timestamp: '%s'; number parser error: %s.", str, err);
  }

  if(*parse_end == 0) {
    *(u64*) dest = (u64) parsed;
    return NULL;
  }

  struct tm parsed_time;
  parse_end = strptime(str, "%F %T", &parsed_time);
  if(parse_end == NULL) {
    return mp_printf(pool, "Invalid value of timestamp: '%s'.", str);
  }
  if(*parse_end != 0) {
    return mp_printf(pool, "Invalid value of timestamp: '%s'.", str);
  }

  time_t tmp_time = mktime(&parsed_time);
  *(u64*)dest = (u64) tmp_time;

  return NULL;
}

TABLE_COL_BODY(timestamp, u64)

const struct xtype xt_timestamp = {
  .size = sizeof(u64),
  .name = "timestamp",
  .parse = xt_timestamp_parse,
  .format = xt_timestamp_format,
  .parse_fmt = xt_timestamp_fmt_parse
};
