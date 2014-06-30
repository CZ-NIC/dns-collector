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

#define TBL_COL_SIZE(_name, _width)           { .name = _name, .width = _width, .fmt = "%llu", .type = COL_TYPE_SIZE }
#define TBL_COL_TIMESTAMP(_name, _width)      { .name = _name, .width = _width, .fmt = "%lld", .type = COL_TYPE_TIMESTAMP }

#define TBL_COL_SIZE_FMT(_name, _width, _units)         { .name = _name, .width = _width, .fmt = "%llu", .type = COL_TYPE_SIZE }
#define TBL_COL_TIMESTAMP_FMT(_name, _width, _fmt)      { .name = _name, .width = _width, .fmt = "%lld", .type = COL_TYPE_TIMESTAMP }

/*
  union {
    enum size_units units;
    enum timestamp_format ts_fmt;
  };
*/
/*
#define TBL_COL_SIZE(_name, _width)           { .name = _name, .width = _width, .fmt = "%llu", .type = COL_TYPE_SIZE }
#define TBL_COL_TIMESTAMP(_name, _width)      { .name = _name, .width = _width, .fmt = "%lld", .type = COL_TYPE_TIMESTAMP }

#define TBL_COL_SIZE_FMT(_name, _width, _units)         { .name = _name, .width = _width, .fmt = "%llu", .type = COL_TYPE_SIZE }
#define TBL_COL_TIMESTAMP_FMT(_name, _width, _fmt)      { .name = _name, .width = _width, .fmt = "%lld", .type = COL_TYPE_TIMESTAMP }

#define TABLE_COL_PROTO(_name_, _type_) void table_col_##_name_(struct table *tbl, int col, _type_ val);\
  void table_col_##_name_##_name(struct table *tbl, const char *col_name, _type_ val);\
  void table_col_##_name_##_fmt(struct table *tbl, int col, _type_ val) FORMAT_CHECK(printf, 3, 0);
//TABLE_COL_PROTO(size, u64);
//TABLE_COL_PROTO(timestamp, u64);
#undef TABLE_COL_PROTO
*/

void table_col_size_name(struct table *tbl, const char *col_name, u64 val);
void table_col_timestamp_name(struct table *tbl, const char * col_name, u64 val);

void table_col_size(struct table *tbl, int col, u64 val);
void table_col_timestamp(struct table *tbl, int col, u64 val);

//TABLE_COL(size, u64, COL_TYPE_SIZE)
//TABLE_COL_STR(size, u64, COL_TYPE_SIZE)

//TABLE_COL(timestamp, u64, COL_TYPE_TIMESTAMP)
//TABLE_COL_STR(timestamp, u64, COL_TYPE_TIMESTAMP)


#endif
