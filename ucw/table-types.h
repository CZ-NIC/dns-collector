#ifndef _UCW_TABLE_TYPES_H
#define _UCW_TABLE_TYPES_H

#include <ucw/table.h>

enum size_units {
  UNIT_BYTE,
  UNIT_KILOBYTE,
  UNIT_MEGABYTE,
  UNIT_GIGABYTE,
  UNIT_TERABYTE,
  UNIT_AUTO
};

enum timestamp_format {
  TIMESTAMP_EPOCH,
  TIMESTAMP_DATETIME
};

#define COL_TYPE_SIZE       COL_TYPE_CUSTOM
#define COL_TYPE_TIMESTAMP  (COL_TYPE_CUSTOM+1)

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
