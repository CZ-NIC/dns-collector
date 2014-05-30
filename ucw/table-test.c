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

static uint test_column_order[] = { test_col3_bool, test_col4_double, test_col2_uint,test_col1_int, test_col0_str };

static struct table test_tbl = {
  TBL_COLUMNS {
    TBL_COL_STR(test, col0_str, 20),
    TBL_COL_INT(test, col1_int, 8),
    TBL_COL_UINT(test, col2_uint, 9),
    TBL_COL_BOOL(test, col3_bool, 9),
    TBL_COL_DOUBLE(test, col4_double, 11, 2),
    TBL_COL_END
  },
  TBL_COL_ORDER(test_column_order),
  TBL_OUTPUT_HUMAN_READABLE,
  TBL_COL_DELIMITER("\t"),
};

enum test_default_order_cols {
  test_default_order_col0_int, test_default_order_col1_int, test_default_order_col2_int
};

static struct table test_default_order_tbl = {
  TBL_COLUMNS {
    TBL_COL_INT(test_default_order, col0_int, 8),
    TBL_COL_INT(test_default_order, col1_int, 9),
    TBL_COL_INT(test_default_order, col2_int, 9),
    TBL_COL_END
  },
  TBL_OUTPUT_HUMAN_READABLE,
  TBL_COL_DELIMITER("\t"),
};

static void do_default_order_test(struct fastbuf *out)
{
  table_init(&test_default_order_tbl, out);
  table_start(&test_default_order_tbl);

  table_set_int(&test_default_order_tbl, test_default_order_col0_int, 0);
  table_set_int(&test_default_order_tbl, test_default_order_col1_int, 1);
  table_set_int(&test_default_order_tbl, test_default_order_col2_int, 2);
  table_end_row(&test_default_order_tbl);

  table_set_int(&test_default_order_tbl, test_default_order_col0_int, 10);
  table_set_int(&test_default_order_tbl, test_default_order_col1_int, 11);
  table_set_int(&test_default_order_tbl, test_default_order_col2_int, 12);
  table_end_row(&test_default_order_tbl);

  table_end(&test_default_order_tbl);
  table_cleanup(&test_default_order_tbl);
}

/**
 * tests: table_set_nt, table_set_uint, table_set_bool, table_set_double, table_set_printf
 **/
static void do_print1(struct table *test_tbl)
{
  table_set_str(test_tbl, test_col0_str, "sdsdf");
  table_append_str(test_tbl, "aaaaa");
  table_set_int(test_tbl, test_col1_int, -10);
  table_set_int(test_tbl, test_col1_int, 10000);
  table_set_uint(test_tbl, test_col2_uint, 10);
  table_set_printf(test_tbl, test_col2_uint, "XXX-%u", 22222);
  table_set_bool(test_tbl, test_col3_bool, 1);
  table_set_double(test_tbl, test_col4_double, 1.5);
  table_end_row(test_tbl);

  table_set_str(test_tbl, test_col0_str, "test");
  table_append_str(test_tbl, "bbbbb");
  table_set_int(test_tbl, test_col1_int, -100);
  table_set_uint(test_tbl, test_col2_uint, 100);
  table_set_bool(test_tbl, test_col3_bool, 0);
  table_set_double(test_tbl, test_col4_double, 1.5);
  table_end_row(test_tbl);
}

static char **cli_table_opts;
static int test_default_column_order;
static int test_invalid_option;
static int test_invalid_order;

static struct opt_section table_printer_opts = {
  OPT_ITEMS {
    OPT_HELP("Options:"),
    OPT_STRING_MULTIPLE('T', "table", cli_table_opts, OPT_REQUIRED_VALUE, "\tSets options for the table."),
    OPT_BOOL('d', 0, test_default_column_order, 0, "\tRun the test that uses the default column order."),
    OPT_BOOL('i', 0, test_invalid_option, 0, "\tTest the output for invalid option."),
    OPT_BOOL('n', 0, test_invalid_order, 0, "\tTest the output for invalid names of columns for the 'cols' option."),
    OPT_END
  }
};

static void process_command_line_opts(char *argv[], struct table *tbl)
{
  GARY_INIT(cli_table_opts, 0);

  opt_parse(&table_printer_opts, argv+1);

  for(uint i = 0; i < GARY_SIZE(cli_table_opts); i++) {
    const char *rv = table_set_option(tbl, cli_table_opts[i]);
    ASSERT_MSG(rv == NULL, "Tableprinter option parser returned error: '%s'.", rv);
  }

  GARY_FREE(cli_table_opts);
}

static bool user_defined_option(struct table *tbl UNUSED, const char *key, const char *value, const char **err UNUSED)
{
  if(value == NULL && strcmp(key, "novaluekey") == 0) {
    printf("setting key: %s; value: (null)\n", key);
    return 1;
  }
  if(value != NULL && strcmp(value, "value") == 0 &&
     key != NULL && strcmp(key, "valuekey") == 0) {
    printf("setting key: %s; value: %s\n", key, value);
    return 1;
  }
  return 0;
}

static void test_option_parser(struct table *tbl)
{
  tbl->formatter->process_option = user_defined_option;
  const char *rv = table_set_option(tbl, "invalid:option");
  if(rv) printf("Tableprinter option parser returned error: \"%s\".\n", rv);

  rv = table_set_option(tbl, "invalid");
  if(rv) printf("Tableprinter option parser returned error: \"%s\".\n", rv);

  rv = table_set_option(tbl, "novaluekey");
  if(rv) printf("Tableprinter option parser returned error: \"%s\".\n", rv);

  rv = table_set_option(tbl, "valuekey:value");
  if(rv) printf("Tableprinter option parser returned error: \"%s\".\n", rv);
}

int main(int argc UNUSED, char **argv)
{
  struct fastbuf *out;
  out = bfdopen_shared(1, 4096);

  table_init(&test_tbl, out);

  process_command_line_opts(argv, &test_tbl);

  if(test_invalid_order == 1) {
    const char *rv = table_set_option(&test_tbl, "cols:test_col0_str,test_col1_int,xxx");
    if(rv) printf("Tableprinter option parser returned: '%s'.\n", rv);
    return 0;
  } else if(test_default_column_order == 1) {
    do_default_order_test(out);
    bclose(out);
    return 0;
  } else if(test_invalid_option == 1) {
    test_option_parser(&test_tbl);
    bclose(out);
    return 0;
  }

  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);
  table_cleanup(&test_tbl);

  bclose(out);

  return 0;
}
