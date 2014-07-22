/*
 *	UCW Library -- Table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#ifndef _UCW_TABLE_TYPES_H
#define _UCW_TABLE_TYPES_H

#include <ucw/table.h>

enum size_units {
  SIZE_UNIT_BYTE,
  SIZE_UNIT_KILOBYTE,
  SIZE_UNIT_MEGABYTE,
  SIZE_UNIT_GIGABYTE,
  SIZE_UNIT_TERABYTE,
  SIZE_UNIT_AUTO
};

#define TIMESTAMP_EPOCH     XTYPE_FMT_RAW
#define TIMESTAMP_DATETIME  XTYPE_FMT_PRETTY

#define SIZE_UNITS_FIXED    0x40000000

extern const struct xtype xt_size;
extern const struct xtype xt_timestamp;

#define TBL_COL_SIZE(_name, _width)       { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_size, .set_col_opt = table_set_col_opt }
#define TBL_COL_TIMESTAMP(_name, _width)  { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_timestamp, .set_col_opt = table_set_col_opt }

#define TBL_COL_SIZE_FMT(_name, _width, _units)    { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_size, .set_col_opt = table_set_col_opt }
#define TBL_COL_TIMESTAMP_FMT(_name, _width, _fmt) { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = &xt_timestamp, .set_col_opt = table_set_col_opt}

TABLE_COL_PROTO(size, u64)
TABLE_COL_PROTO(timestamp, u64)

#endif
