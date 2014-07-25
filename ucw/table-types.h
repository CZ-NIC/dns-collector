/*
 *	UCW Library -- Table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#ifndef _UCW_TABLE_TYPES_H
#define _UCW_TABLE_TYPES_H

#include <ucw/table.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define table_col_size ucw_table_col_size
#define table_col_timestamp ucw_table_col_timestamp
#define xt_size ucw_xt_size
#define xt_timestamp ucw_xt_timestamp
#endif

/* Size, possibly with a unit. Internally represented as u64. */

extern const struct xtype xt_size;

enum size_units {
  XT_SIZE_UNIT_BYTE,
  XT_SIZE_UNIT_KILOBYTE,
  XT_SIZE_UNIT_MEGABYTE,
  XT_SIZE_UNIT_GIGABYTE,
  XT_SIZE_UNIT_TERABYTE,
  XT_SIZE_UNIT_AUTO
};

#define XT_SIZE_FMT_UNIT(_unit) (_unit | XT_SIZE_FMT_FIXED_UNIT)
#define XT_SIZE_FMT_FIXED_UNIT XTYPE_FMT_CUSTOM

#define TBL_COL_SIZE(_name, _width)       { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_size, .set_col_opt = table_set_col_opt }
#define TBL_COL_SIZE_FMT(_name, _width, _fmt)      { .name = _name, .width = _width, .fmt = _fmt, .type_def = &xt_size, .set_col_opt = table_set_col_opt }

TABLE_COL_PROTO(size, u64)

/* Timestamp. Internally represented as time_t. */

#define XT_TIMESTAMP_FMT_EPOCH     XTYPE_FMT_RAW
#define XT_TIMESTAMP_FMT_DATETIME  XTYPE_FMT_PRETTY

extern const struct xtype xt_timestamp;

#define TBL_COL_TIMESTAMP(_name, _width)  { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_timestamp, .set_col_opt = table_set_col_opt }
#define TBL_COL_TIMESTAMP_FMT(_name, _width, _fmt) { .name = _name, .width = _width, .fmt = _fmt, .type_def = &xt_timestamp, .set_col_opt = table_set_col_opt }

TABLE_COL_PROTO(timestamp, u64)

#endif
