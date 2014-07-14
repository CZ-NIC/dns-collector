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

#define COL_TYPE_SIZE       &xt_size
#define COL_TYPE_TIMESTAMP  &xt_timestamp

#define TIMESTAMP_EPOCH     XTYPE_FMT_RAW
#define TIMESTAMP_DATETIME  XTYPE_FMT_PRETTY

#define SIZE_UNITS_FIXED    0x40000000

extern const struct xtype xt_size;
extern const struct xtype xt_timestamp;

bool table_set_col_opt_size(struct table *tbl, uint col_inst_idx, const char *col_arg, char **err);
bool table_set_col_opt_timestamp(struct table *tbl, uint col_inst_idx, const char *col_arg, char **err);

#define TBL_COL_SIZE(_name, _width)           { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = COL_TYPE_SIZE, .set_col_instance_option = table_set_col_opt_size }
#define TBL_COL_TIMESTAMP(_name, _width)      { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = COL_TYPE_TIMESTAMP, .set_col_instance_option = table_set_col_opt_timestamp }

#define TBL_COL_SIZE_FMT(_name, _width, _units)         { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = COL_TYPE_SIZE, .set_col_instance_option = table_set_col_opt_size }
#define TBL_COL_TIMESTAMP_FMT(_name, _width, _fmt)      { .name = _name, .width = _width, .fmt = XTYPE_FMT_DEFAULT, .type_def = COL_TYPE_TIMESTAMP, .set_col_instance_option = table_set_col_opt_timestamp }

void table_col_size_name(struct table *tbl, const char *col_name, u64 val);
void table_col_timestamp_name(struct table *tbl, const char * col_name, u64 val);

void table_col_size(struct table *tbl, int col, u64 val);
void table_col_timestamp(struct table *tbl, int col, u64 val);

#endif
