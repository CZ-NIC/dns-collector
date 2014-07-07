/*
 *	Unit tests of table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#include <ucw/lib.h>
#include <ucw/table.h>
#include <ucw/opt.h>
#include <stdio.h>


enum test_table_cols {
  test_col0_str, test_col1_int, test_col2_uint, test_col3_bool, test_col4_double
};

static struct table test_tbl = {
  TBL_COLUMNS {
    [test_col0_str] = TBL_COL_STR("col0_str", 30 | CELL_ALIGN_LEFT),
    [test_col1_int] = TBL_COL_INT("col1_int", 8),
    [test_col2_uint] = TBL_COL_UINT("col2_uint", 9),
    [test_col3_bool] = TBL_COL_BOOL("col3_bool", 9 | CELL_ALIGN_LEFT),
    [test_col4_double] = TBL_COL_DOUBLE("col4_double", 11 | CELL_ALIGN_LEFT, 5),
    TBL_COL_END
  },
  TBL_OUTPUT_HUMAN_READABLE,
  TBL_COL_DELIMITER("\t"),
};

static int test_to_perform = -1;
static char **cli_table_opts;

static struct opt_section table_printer_opts = {
  OPT_ITEMS {
    OPT_HELP("Options:"),
    OPT_STRING_MULTIPLE('T', "table", cli_table_opts, OPT_REQUIRED_VALUE, "\tSets options for the table."),
    OPT_END
  }
};


static void process_command_line_opts(char *argv[], struct table *tbl)
{
  GARY_INIT(cli_table_opts, 0);

  opt_parse(&table_printer_opts, argv+1);
  table_set_gary_options(tbl, cli_table_opts);

  GARY_FREE(cli_table_opts);
}

static void print_table(struct table *tbl, struct fastbuf *out)
{
  table_start(tbl, out);

  struct fastbuf *colfb = table_col_fbstart(tbl, test_col0_str);
  bputs(colfb, "HELLO");
  bprintf(colfb, ",col_idx:%d", test_col0_str);
  table_col_fbend(tbl);

  table_col_int(tbl, test_col1_int, -10);
  table_col_uint(tbl, test_col2_uint, 10);
  table_col_bool(tbl, test_col3_bool, 0);
  table_col_double(tbl, test_col4_double, 3.1415926535897);
  table_end_row(tbl);



  colfb = table_col_fbstart(tbl, test_col0_str);
  bputs(colfb, "EHLO");
  bprintf(colfb, ",col_idx:%d", test_col0_str);
  table_col_fbend(tbl);

  table_col_int(tbl, test_col1_int, -12345);
  table_col_uint(tbl, test_col2_uint, 0xFF);
  table_col_bool(tbl, test_col3_bool, 1);
  table_col_double(tbl, test_col4_double, 1.61803398875);
  table_end_row(tbl);



  colfb = table_col_fbstart(tbl, test_col0_str);
  bputs(colfb, "AHOJ");
  bprintf(colfb, ",col_idx:%d", test_col0_str);
  table_col_fbend(tbl);

  table_col_int(tbl, test_col1_int, -54321);
  table_col_uint(tbl, test_col2_uint, 0xFF00);
  table_col_bool(tbl, test_col3_bool, 0);
  table_col_double(tbl, test_col4_double, 2.718281828459045);
  table_end_row(tbl);

  table_end(tbl);
}


int main(int argc UNUSED, char **argv)
{
  struct fastbuf *out;
  out = bfdopen_shared(1, 4096);

  struct table *tbl = table_init(&test_tbl);
  process_command_line_opts(argv, tbl);

  print_table(tbl, out);
  table_cleanup(tbl);
  bclose(out);

  return 0;
}

