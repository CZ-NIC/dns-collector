/*
 *	UCW Library -- Table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#ifndef _UCW_TABLE_TYPES_H
#define _UCW_TABLE_TYPES_H

#include <ucw/table.h>

enum size_units {
  SIZE_UNIT_BYTE = CELL_OUT_USER_DEF_START,
  SIZE_UNIT_KILOBYTE,
  SIZE_UNIT_MEGABYTE,
  SIZE_UNIT_GIGABYTE,
  SIZE_UNIT_TERABYTE,
  SIZE_UNIT_AUTO
};

enum timestamp_format {
  TIMESTAMP_EPOCH,
  TIMESTAMP_DATETIME
};

#define COL_TYPE_SIZE       COL_TYPE_UCW
#define COL_TYPE_TIMESTAMP  (COL_TYPE_UCW+1)

extern struct table_user_type table_type_timestamp;
extern struct table_user_type table_type_size;

#define TBL_COL_SIZE(_name, _width)           { .name = _name, .width = _width, .fmt = "%llu", .type = COL_TYPE_SIZE, .type_def = &table_type_size }
#define TBL_COL_TIMESTAMP(_name, _width)      { .name = _name, .width = _width, .fmt = "%lld", .type = COL_TYPE_TIMESTAMP, .type_def = &table_type_timestamp }

#define TBL_COL_SIZE_FMT(_name, _width, _units)         { .name = _name, .width = _width, .fmt = "%llu", .type = COL_TYPE_SIZE, .type_def = &table_type_size }
#define TBL_COL_TIMESTAMP_FMT(_name, _width, _fmt)      { .name = _name, .width = _width, .fmt = "%lld", .type = COL_TYPE_TIMESTAMP, .type_def = &table_type_timestamp }

void table_col_size_name(struct table *tbl, const char *col_name, u64 val);
void table_col_timestamp_name(struct table *tbl, const char * col_name, u64 val);

void table_col_size(struct table *tbl, int col, u64 val);
void table_col_timestamp(struct table *tbl, int col, u64 val);

#endif
