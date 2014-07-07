/*
 *	Unit tests of table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#include <ucw/lib.h>
#include <ucw/table.h>
#include <ucw/table-types.h>
#include <ucw/opt.h>
#include <stdio.h>

enum test_table_cols {
  TEST_COL0_SIZE, TEST_COL1_TS
};

static struct table test_tbl = {
  TBL_COLUMNS {
    [TEST_COL0_SIZE] = TBL_COL_SIZE_FMT("size", 15, UNIT_BYTE),
    [TEST_COL1_TS] = TBL_COL_TIMESTAMP("ts", 20),
    TBL_COL_END
  },
  TBL_OUTPUT_HUMAN_READABLE,
};

static void do_test(void)
{
  struct fastbuf *out;
  out = bfdopen_shared(1, 4096);
  table_init(&test_tbl);
  table_start(&test_tbl, out);

  u64 test_time = 1403685533;
  s64 test_size = 4LU*(1024LU * 1024LU * 1024LU);

  table_col_size(&test_tbl, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl);

  test_tbl.column_order[TEST_COL0_SIZE].output_type = UNIT_KILOBYTE;
  table_col_size(&test_tbl, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl);

  test_tbl.column_order[TEST_COL0_SIZE].output_type = UNIT_MEGABYTE;
  table_col_size(&test_tbl, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl);

  test_tbl.column_order[TEST_COL0_SIZE].output_type = UNIT_GIGABYTE;
  test_tbl.column_order[TEST_COL1_TS].output_type = TIMESTAMP_DATETIME;
  table_col_size(&test_tbl, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl);

  test_size = test_size * 1024LU;
  test_tbl.column_order[TEST_COL0_SIZE].output_type = UNIT_TERABYTE;
  test_tbl.column_order[TEST_COL1_TS].output_type = TIMESTAMP_DATETIME;
  table_col_size(&test_tbl, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl);

  table_end(&test_tbl);
  table_cleanup(&test_tbl);

  bclose(out);
}



static struct table test_tbl2 = {
  TBL_COLUMNS {
    [TEST_COL0_SIZE] = TBL_COL_SIZE_FMT("size", 15, UNIT_BYTE),
    [TEST_COL1_TS] = TBL_COL_TIMESTAMP("ts", 20),
    TBL_COL_END
  },
  TBL_OUTPUT_HUMAN_READABLE,
};

static void do_test2(void)
{
  struct fastbuf *out;
  out = bfdopen_shared(1, 4096);
  table_init(&test_tbl2);
  table_set_col_order_by_name(&test_tbl2, "");
  const char *err = table_set_option_value(&test_tbl2, "cols", "size[kb],size[mb],size[gb],size[tb],ts[datetime],ts[timestamp]");
  if(err) {
    opt_failure("err in table_set_option_value: '%s'.", err);
    abort();
  }
  table_start(&test_tbl2, out);

  u64 test_time = 1403685533;
  s64 test_size = 4LU*(1024LU * 1024LU * 1024LU);

  table_col_size(&test_tbl2, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl2, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl2);

  table_col_size(&test_tbl2, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl2, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl2);

  test_size = test_size * 1024LU;

  table_col_size(&test_tbl2, TEST_COL0_SIZE, test_size);
  table_col_timestamp(&test_tbl2, TEST_COL1_TS, test_time);
  table_end_row(&test_tbl2);

  table_end(&test_tbl2);
  table_cleanup(&test_tbl2);

  bclose(out);
}

int main(int argc UNUSED, char **argv UNUSED)
{
  do_test();
  do_test2();

  return 0;
}

