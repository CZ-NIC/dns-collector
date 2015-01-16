/*
 *	UCW Library -- Extended types - extra types
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#ifndef _UCW_XTYPES_EXTRA_H
#define _UCW_XTYPES_EXTRA_H

#include <ucw/xtypes.h>
#include <ucw/table.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define table_col_size ucw_table_col_size
#define table_col_timestamp ucw_table_col_timestamp
#define xt_size ucw_xt_size
#define xt_timestamp ucw_xt_timestamp
#endif

/***
 * Size
 * ~~~~
 *
 * `xt_size` is a size, possibly with a unit. Internally, it is represented
 * as a `u64`.
 ***/

extern const struct xtype xt_size;

/** Units **/
enum size_units {
  XT_SIZE_UNIT_BYTE,
  XT_SIZE_UNIT_KILOBYTE,
  XT_SIZE_UNIT_MEGABYTE,
  XT_SIZE_UNIT_GIGABYTE,
  XT_SIZE_UNIT_TERABYTE,
  XT_SIZE_UNIT_AUTO
};

/**
 * Custom formatting mode: use a specified unit (`XT_SIZE_UNIT_`'xxx').
 * Textual representation of the mode is the name of the unit (case-insensitive).
 **/
#define XT_SIZE_FMT_UNIT(_unit) (_unit | XT_SIZE_FMT_FIXED_UNIT)
#define XT_SIZE_FMT_FIXED_UNIT XTYPE_FMT_CUSTOM

#define TBL_COL_SIZE(_name, _width)       { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_size }
#define TBL_COL_SIZE_FMT(_name, _width, _fmt)      { .name = _name, .width = _width, .fmt = _fmt, .type_def = &xt_size }

TABLE_COL_PROTO(size, u64)

/***
 * Time
 * ~~~~
 *
 * `xt_timestamp` is a timestamp, internally represented as `time_t`.
 ***/

/**
 * Custom formatting mode: seconds since Unix epoch. Currently,
 * this is the same as the raw format. Textual representation: `timestamp` or `epoch`.
 **/
#define XT_TIMESTAMP_FMT_EPOCH     XTYPE_FMT_RAW

/**
 * Custom formatting mode: date and time. Currently, this is the same
 * as the human-readable format. Textual representation: `datetime`.
 **/
#define XT_TIMESTAMP_FMT_DATETIME  XTYPE_FMT_PRETTY

extern const struct xtype xt_timestamp;

#define TBL_COL_TIMESTAMP(_name, _width)  { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_timestamp }
#define TBL_COL_TIMESTAMP_FMT(_name, _width, _fmt) { .name = _name, .width = _width, .fmt = _fmt, .type_def = &xt_timestamp }

TABLE_COL_PROTO(timestamp, u64)

#endif
