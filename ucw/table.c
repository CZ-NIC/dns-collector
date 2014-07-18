/*
 *	UCW Library -- Table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#include <ucw/lib.h>
#include <ucw/string.h>
#include <ucw/stkstring.h>
#include <ucw/gary.h>
#include <ucw/table.h>
#include <ucw/strtonum.h>

#include <stdlib.h>
#include <stdio.h>

/* Forward declarations */

static void table_update_ll(struct table *tbl);

/*** Management of tables ***/

static struct table *table_make_instance(const struct table_template *tbl_template)
{
  struct mempool *pool = mp_new(4096);
  struct table *new_inst = mp_alloc_zero(pool, sizeof(struct table));

  new_inst->pool = pool;

  // initialize column definitions
  uint col_count = 0; // count the number of columns in the struct table
  for(;;) {
    if(tbl_template->columns[col_count].name == NULL &&
       tbl_template->columns[col_count].width == 0 &&
       tbl_template->columns[col_count].type_def == COL_TYPE_ANY)
      break;
    ASSERT(tbl_template->columns[col_count].name != NULL);
    ASSERT(tbl_template->columns[col_count].width != 0);

    col_count++;
  }
  new_inst->column_count = col_count;

  new_inst->columns = tbl_template->columns;
  new_inst->ll_headers = mp_alloc(new_inst->pool, sizeof(int) * col_count);
  for(uint i = 0; i < col_count; i++) {
    new_inst->ll_headers[i] = -1;
  }

  // initialize column_order
  if(tbl_template->column_order) {
    new_inst->column_order = mp_alloc_zero(new_inst->pool, sizeof(struct table_col_instance) * tbl_template->cols_to_output);
    memcpy(new_inst->column_order, tbl_template->column_order, sizeof(struct table_col_instance) * tbl_template->cols_to_output);
    for(uint i = 0; i < new_inst->cols_to_output; i++) {
      new_inst->column_order[i].cell_content = NULL;
      int col_idx = new_inst->column_order[i].idx;
      new_inst->column_order[i].col_def = new_inst->columns + col_idx;
      new_inst->column_order[i].output_type = tbl_template->columns[col_idx].fmt;
    }

    new_inst->cols_to_output = tbl_template->cols_to_output;
  }

  new_inst->col_delimiter = tbl_template->col_delimiter;
  new_inst->print_header = 1;
  new_inst->out = 0;
  new_inst->last_printed_col = -1;
  new_inst->row_printing_started = 0;
  new_inst->col_out = -1;
  new_inst->formatter = tbl_template->formatter;
  new_inst->data = NULL;
  return new_inst;
}

struct table *table_init(const struct table_template *tbl_template)
{
  struct table *tbl = table_make_instance(tbl_template);

  if(!tbl->formatter) {
    tbl->formatter = &table_fmt_human_readable;
  }

  tbl->print_header = 1; // by default, print header
  return tbl;
}

void table_cleanup(struct table *tbl)
{
  mp_delete(tbl->pool);
}

// TODO: test default column order
static void table_make_default_column_order(struct table *tbl)
{
  int *col_order_int = mp_alloc_zero(tbl->pool, sizeof(int) * tbl->column_count); // FIXME: use stack instead of memory pool
  for(int i = 0; i < tbl->column_count; i++) {
    col_order_int[i] = i;
  }
  table_set_col_order(tbl, col_order_int, tbl->column_count);
}

void table_start(struct table *tbl, struct fastbuf *out)
{
  tbl->last_printed_col = -1;
  tbl->row_printing_started = 0;
  tbl->out = out;

  ASSERT_MSG(tbl->out, "Output fastbuf not specified.");

  if(tbl->column_order == NULL) table_make_default_column_order(tbl);
  else {
    // update linked lists
    table_update_ll(tbl);
  }
  if(tbl->formatter->table_start != NULL) tbl->formatter->table_start(tbl);

  mp_save(tbl->pool, &tbl->pool_state);

  ASSERT_MSG(tbl->col_delimiter, "In-between column delimiter not specified.");
}

void table_end(struct table *tbl)
{
  tbl->last_printed_col = -1;
  tbl->row_printing_started = 0;

  mp_restore(tbl->pool, &tbl->pool_state);

  if(tbl->formatter->table_end) tbl->formatter->table_end(tbl);
}

/*** Configuration ***/

void table_set_formatter(struct table *tbl, struct table_formatter *fmt)
{
  tbl->formatter = fmt;
}

int table_get_col_idx(struct table *tbl, const char *col_name)
{
  for(int i = 0; i < tbl->column_count; i++) {
    if(strcmp(tbl->columns[i].name, col_name) == 0) return i;
  }
  return -1;
}

const char * table_get_col_list(struct table *tbl)
{
  if(tbl->column_count == 0) return "";

  char *tmp = mp_strdup(tbl->pool, tbl->columns[0].name);

  for(int i = 1; i < tbl->column_count; i++) {
    tmp = mp_printf_append(tbl->pool, tmp, ", %s", tbl->columns[i].name);
  }

  return tmp;
}

static void table_update_ll(struct table *tbl)
{
  int cols_to_output = tbl->cols_to_output;

  for(int i = 0; i < tbl->column_count; i++) {
    tbl->ll_headers[i] = -1;
  }

  for(int i = 0; i < cols_to_output; i++) {
    int idx = tbl->column_order[i].idx;
    tbl->column_order[i].col_def = tbl->columns + idx;
  }

  for(int i = 0; i < cols_to_output; i++) {
    int col_def_idx = tbl->column_order[i].col_def - tbl->columns;
    int first = tbl->ll_headers[col_def_idx];
    tbl->ll_headers[col_def_idx] = i;
    tbl->column_order[i].next_column = first;
  }
}

void table_set_col_order(struct table *tbl, int *col_order, int cols_to_output)
{
  for(int i = 0; i < cols_to_output; i++) {
    ASSERT_MSG(col_order[i] >= 0 && col_order[i] < tbl->column_count, "Column %d does not exist (column number should be between 0 and %d).", col_order[i], tbl->column_count - 1);
  }

  tbl->cols_to_output = cols_to_output;
  tbl->column_order = mp_alloc_zero(tbl->pool, sizeof(struct table_col_instance) * cols_to_output);
  for(int i = 0; i < cols_to_output; i++) {
    int col_idx = col_order[i];
    tbl->column_order[i].idx = col_idx;
    tbl->column_order[i].col_def = tbl->columns + col_idx;
    tbl->column_order[i].cell_content = NULL;
    tbl->column_order[i].output_type = tbl->columns[col_idx].fmt;
  }
  table_update_ll(tbl);
}

bool table_col_is_printed(struct table *tbl, uint col_idx)
{
  if(tbl->ll_headers[col_idx] == -1) return 0;

  return 1;
}

static char * table_parse_col_arg(char *col_def)
{
  // FIXME: should be switched to str_sepsplit
  char * left_br = strchr(col_def, '[');
  if(left_br == NULL) return NULL;
  *left_br = 0;
  left_br++;
  char *right_br = strchr(left_br, ']');
  *right_br = 0;
  return left_br;
}

/**
 * Setting options for basic table types (as defined in table.h)
 **/
int table_set_col_opt_default(struct table *tbl, int col_idx, const char *col_arg, char **err)
{
  const struct table_column *col_def = tbl->column_order[col_idx].col_def;

  if(col_def->type_def == &xt_double) {
    uint precision = 0;
    const char *tmp_err = str_to_uint(&precision, col_arg, NULL, 0);
    if(tmp_err) {
      *err = mp_printf(tbl->pool, "An error occured while parsing precision: %s.", tmp_err);
      return false;
    }
    tbl->column_order[col_idx].output_type = precision; // FIXME: shift the value of precision
    return true;
  }

  *err = mp_printf(tbl->pool, "Invalid column format option: '%s' for column %d.", col_arg, col_idx);
  return false;
}

/**
 * TODO: This function deliberately leaks memory. When it is called multiple times,
 * previous column orders still remain allocated in the table's memory pool.
 **/
const char * table_set_col_order_by_name(struct table *tbl, const char *col_order_str)
{
  if(col_order_str[0] == '*') {
    int *col_order_int = alloca(sizeof(int) * tbl->column_count);
    for(int i = 0; i < tbl->column_count; i++) {
      col_order_int[i] = i;
    }
    table_set_col_order(tbl, col_order_int, tbl->column_count);

    return NULL;
  }

  if(!col_order_str[0]) {
    tbl->column_order = mp_alloc(tbl->pool, 0);
    tbl->cols_to_output = 0;
    return NULL;
  }

  char *tmp_col_order = stk_strdup(col_order_str);

  int col_count = 1;
  for(int i = 0; col_order_str[i] != 0; i++) {
    if(col_order_str[i] == ',') {
      col_count++;
    }
  }

  tbl->cols_to_output = col_count;
  tbl->column_order = mp_alloc_zero(tbl->pool, sizeof(struct table_col_instance) * col_count);

  int curr_col_idx = 0;
  char *name_start = tmp_col_order;
  while(name_start) {
    char *next = strchr(name_start, ',');
    if(next) {
      *next++ = 0;
    }

    char *arg = table_parse_col_arg(name_start); // this sets 0 on the '['
    int col_idx = table_get_col_idx(tbl, name_start);

    if(col_idx == -1) {
      return mp_printf(tbl->pool, "Unknown table column '%s', possible column names are: %s.", name_start, table_get_col_list(tbl));
    }
    tbl->column_order[curr_col_idx].col_def = tbl->columns + col_idx;
    tbl->column_order[curr_col_idx].idx = col_idx;
    tbl->column_order[curr_col_idx].cell_content = NULL;
    tbl->column_order[curr_col_idx].output_type = tbl->columns[col_idx].fmt;
    if(tbl->columns[col_idx].type_def && tbl->columns[col_idx].set_col_instance_option) {
      char *err = NULL;
      tbl->columns[col_idx].set_col_instance_option(tbl, curr_col_idx, arg, &err);
      if(err) return mp_printf(tbl->pool, "Error occured while setting column option: %s.", err);
    }

    name_start = next;
    curr_col_idx++;
  }

  table_update_ll(tbl);

  return NULL;
}

/*** Table cells ***/

static void table_set_all_inst_content(struct table *tbl, int col_templ, const char *col_content)
{
  TBL_COL_ITER_START(tbl, col_templ, curr_col_ptr, curr_col) {
    curr_col_ptr->cell_content = col_content;
  } TBL_COL_ITER_END
}

void table_col_generic_format(struct table *tbl, int col, void *value, const struct xtype *expected_type)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(tbl->columns[col].type_def == COL_TYPE_ANY || expected_type == tbl->columns[col].type_def);
  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;
  const char *cell_content = NULL;
  TBL_COL_ITER_START(tbl, col, curr_col, curr_col_idx) {
    enum xtype_fmt fmt = curr_col->output_type;
    cell_content = expected_type->format(value, fmt, tbl->pool);
    curr_col->cell_content = cell_content;
  } TBL_COL_ITER_END
}

void table_col_printf(struct table *tbl, int col, const char *fmt, ...)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;
  va_list args;
  va_start(args, fmt);
  char *cell_content = mp_vprintf(tbl->pool, fmt, args);
  table_set_all_inst_content(tbl, col, cell_content);
  va_end(args);
}

TABLE_COL_BODY(int, int)
TABLE_COL_BODY(uint, uint)
TABLE_COL_BODY(double, double)
TABLE_COL_BODY(intmax, intmax_t)
TABLE_COL_BODY(uintmax, uintmax_t)
TABLE_COL_BODY(s64, s64)
TABLE_COL_BODY(u64, u64)
TABLE_COL_BODY(bool, bool)

void table_col_str(struct table *tbl, int col, const char *val) {
  table_col_generic_format(tbl, col, (void*)val, &xt_str);
}

void table_reset_row(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    tbl->column_order[i].cell_content = NULL;
  }
  mp_restore(tbl->pool, &tbl->pool_state);
  tbl->last_printed_col = -1;
  tbl->row_printing_started = 0;
}

void table_end_row(struct table *tbl)
{
  ASSERT(tbl->formatter->row_output);
  if(tbl->row_printing_started == 0) return;
  tbl->formatter->row_output(tbl);
  table_reset_row(tbl);
}

/* Construction of a cell using a fastbuf */

struct fastbuf *table_col_fbstart(struct table *tbl, int col)
{
  fbpool_init(&tbl->fb_col_out);
  fbpool_start(&tbl->fb_col_out, tbl->pool, 1);
  tbl->col_out = col;
  return &tbl->fb_col_out.fb;
}

void table_col_fbend(struct table *tbl)
{
  char *cell_content = fbpool_end(&tbl->fb_col_out);
  table_set_all_inst_content(tbl, tbl->col_out, cell_content);
  tbl->col_out = -1;
}

/*** Option parsing ***/

const char *table_set_option_value(struct table *tbl, const char *key, const char *value)
{
  // Options with no value
  if(value == NULL || (value != NULL && strlen(value) == 0)) {
    if(strcmp(key, "noheader") == 0) {
      tbl->print_header = 0;
      return NULL;
    }
  }

  // Options with a value
  if(value) {
    if(strcmp(key, "header") == 0) {
      if(value[1] != 0)
        return mp_printf(tbl->pool, "Invalid header parameter: '%s' has invalid value: '%s'.", key, value);
      uint tmp = value[0] - '0';
      if(tmp > 1)
        return mp_printf(tbl->pool, "Invalid header parameter: '%s' has invalid value: '%s'.", key, value);
      tbl->print_header = tmp;
      return NULL;
    } else if(strcmp(key, "cols") == 0) {
      return table_set_col_order_by_name(tbl, value);
    } else if(strcmp(key, "fmt") == 0) {
      if(strcmp(value, "human") == 0) table_set_formatter(tbl, &table_fmt_human_readable);
      else if(strcmp(value, "machine") == 0) table_set_formatter(tbl, &table_fmt_machine_readable);
      else if(strcmp(value, "blockline") == 0) table_set_formatter(tbl, &table_fmt_blockline);
      else {
        return "Invalid argument to output-type option.";
      }
      return NULL;
    } else if(strcmp(key, "cell-fmt") == 0) {
      u32 fmt = 0;
      const char *err = xtype_parse_fmt(NULL, value, &fmt, tbl->pool);
      if(err) return mp_printf(tbl->pool, "Invalid cell format: '%s'.", err);
      for(uint i = 0; i < tbl->cols_to_output; i++) {
        tbl->column_order[i].output_type = fmt;
      }
      return NULL;
    } else if(strcmp(key, "col-delim") == 0) {
      char * d = mp_printf(tbl->pool, "%s", value);
      tbl->col_delimiter = d;
      return NULL;
    }
  }

  // Formatter options
  if(tbl->formatter && tbl->formatter->process_option) {
    const char *err = NULL;
    if(tbl->formatter->process_option(tbl, key, value, &err)) {
      return err;
    }
  }

  // Unrecognized option
  return mp_printf(tbl->pool, "Invalid option: '%s%s%s'.", key, (value ? ":" : ""), (value ? : ""));
}

const char *table_set_option(struct table *tbl, const char *opt)
{
  char *key = stk_strdup(opt);
  char *value = strchr(key, ':');
  if(value) {
    *value++ = 0;
  }
  return table_set_option_value(tbl, key, value);
}

const char *table_set_gary_options(struct table *tbl, char **gary_table_opts)
{
  for (uint i = 0; i < GARY_SIZE(gary_table_opts); i++) {
    const char *rv = table_set_option(tbl, gary_table_opts[i]);
    if(rv != NULL) {
      return rv;
    }
  }
  return NULL;
}

/*** Default formatter for human-readable output ***/

static void table_row_human_readable(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    const struct table_column *col_def = tbl->column_order[i].col_def;
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    int col_width = col_def->width & CELL_WIDTH_MASK;
    if(col_def->width & CELL_ALIGN_LEFT) col_width = -1 * col_width;
    bprintf(tbl->out, "%*s", col_width, tbl->column_order[i].cell_content);
  }
  bputc(tbl->out, '\n');
}

static void table_write_header(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    const struct table_column *col_def = tbl->column_order[i].col_def;
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    int col_width = col_def->width & CELL_WIDTH_MASK;
    if(col_def->width & CELL_ALIGN_LEFT) col_width = -1 * col_width;
    bprintf(tbl->out, "%*s", col_width, col_def->name);
  }
  bputc(tbl->out, '\n');
}

static void table_start_human_readable(struct table *tbl)
{
  if(tbl->col_delimiter == NULL) {
    tbl->col_delimiter = " ";
  }

  if(tbl->print_header != 0) {
    table_write_header(tbl);
  }
}

struct table_formatter table_fmt_human_readable = {
  .row_output = table_row_human_readable,
  .table_start = table_start_human_readable,
};

/*** Default formatter for machine-readable output ***/

static void table_row_machine_readable(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    bputs(tbl->out, tbl->column_order[i].cell_content);
  }
  bputc(tbl->out, '\n');
}

static void table_start_machine_readable(struct table *tbl)
{
  if(tbl->col_delimiter == NULL) {
    tbl->col_delimiter = "\t";
  }

  if(tbl->print_header != 0 && tbl->cols_to_output > 0) {
    bputs(tbl->out, tbl->column_order[0].col_def->name);
    for(uint i = 1; i < tbl->cols_to_output; i++) {
      bputs(tbl->out, tbl->col_delimiter);
      bputs(tbl->out, tbl->column_order[i].col_def->name);
    }
    bputc(tbl->out, '\n');
  }
}

struct table_formatter table_fmt_machine_readable = {
  .row_output = table_row_machine_readable,
  .table_start = table_start_machine_readable,
};


/*** Blockline formatter ***/

static void table_row_blockline_output(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    const struct table_column *col_def = tbl->column_order[i].col_def;
    bprintf(tbl->out, "%s: %s\n", col_def->name, tbl->column_order[i].cell_content);
  }
  bputc(tbl->out, '\n');
}

static void table_start_blockline(struct table *tbl)
{
  if(tbl->col_delimiter == NULL) {
    tbl->col_delimiter = "\n";
  }
}

struct table_formatter table_fmt_blockline = {
  .row_output = table_row_blockline_output,
  .table_start = table_start_blockline
};

/*** Tests ***/

#ifdef TEST

#include <stdio.h>

enum test_table_cols {
  TEST_COL0_STR, TEST_COL1_INT, TEST_COL2_UINT, TEST_COL3_BOOL, TEST_COL4_DOUBLE
};

static struct table_col_instance test_column_order[] = { TBL_COL(TEST_COL3_BOOL), TBL_COL(TEST_COL4_DOUBLE), TBL_COL(TEST_COL2_UINT), TBL_COL(TEST_COL1_INT), TBL_COL(TEST_COL0_STR) };

static struct table_template test_tbl = {
  TBL_COLUMNS {
    [TEST_COL0_STR] = TBL_COL_STR("col0_str", 20),
    [TEST_COL1_INT] = TBL_COL_INT("col1_int", 8),
    [TEST_COL2_UINT] = TBL_COL_UINT("col2_uint", 9),
    [TEST_COL3_BOOL] = TBL_COL_BOOL("col3_bool", 9),
    [TEST_COL4_DOUBLE] = TBL_COL_DOUBLE("col4_double", 11),
    TBL_COL_END
  },
  TBL_COL_ORDER(test_column_order),
  TBL_OUTPUT_HUMAN_READABLE,
  TBL_COL_DELIMITER("\t"),
};

/**
 * tests: table_set_nt, table_set_uint, table_set_bool, table_set_double, table_set_printf
 **/
static void do_print1(struct table *test_tbl)
{
  table_col_str(test_tbl, TEST_COL0_STR, "sdsdf");
  table_col_int(test_tbl, TEST_COL1_INT, -10);
  table_col_int(test_tbl, TEST_COL1_INT, 10000);
  table_col_uint(test_tbl, TEST_COL2_UINT, 10);
  table_col_printf(test_tbl, TEST_COL2_UINT, "XXX-%u", 22222);
  table_col_bool(test_tbl, TEST_COL3_BOOL, true);
  table_col_double(test_tbl, TEST_COL4_DOUBLE, 1.5);
  table_col_printf(test_tbl, TEST_COL4_DOUBLE, "AAA");
  table_end_row(test_tbl);

  table_col_str(test_tbl, TEST_COL0_STR, "test");
  table_col_int(test_tbl, TEST_COL1_INT, -100);
  table_col_uint(test_tbl, TEST_COL2_UINT, 100);
  table_col_bool(test_tbl, TEST_COL3_BOOL, false);
  table_col_printf(test_tbl, TEST_COL4_DOUBLE, "%.2lf", 1.5);
  table_end_row(test_tbl);
}

static void test_simple1(struct fastbuf *out)
{
  struct table *tbl = table_init(&test_tbl);

  // print table with header
  table_set_col_order_by_name(tbl, "col3_bool");
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);

  // print the same table as in the previous case without header
  table_set_col_order_by_name(tbl, "col0_str,col2_uint,col1_int,col3_bool");
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);

  // this also tests whether there is need to call table_set_col_order_by_name after table_end was called
  tbl->print_header = 0;
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);
  tbl->print_header = 1;

  table_set_col_order_by_name(tbl, "col3_bool");
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);

  table_set_col_order_by_name(tbl, "col3_bool,col0_str");
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);

  table_set_col_order_by_name(tbl, "col0_str,col3_bool,col2_uint");
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);

  table_set_col_order_by_name(tbl, "col0_str,col3_bool,col2_uint,col0_str,col3_bool,col2_uint,col0_str,col3_bool,col2_uint");
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);

  table_set_col_order_by_name(tbl, "col0_str,col1_int,col2_uint,col3_bool,col4_double");
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);

  table_cleanup(tbl);
}

enum test_any_table_cols {
  TEST_ANY_COL0_INT, TEST_ANY_COL1_ANY
};

static struct table_col_instance test_any_column_order[] = { TBL_COL(TEST_ANY_COL0_INT), TBL_COL_FMT(TEST_ANY_COL1_ANY, XTYPE_FMT_PRETTY) };

static struct table_template test_any_tbl = {
  TBL_COLUMNS {
    [TEST_ANY_COL0_INT] = TBL_COL_INT("col0_int", 8),
    [TEST_ANY_COL1_ANY] = TBL_COL_ANY_FMT("col1_any", 9, XTYPE_FMT_PRETTY),
    TBL_COL_END
  },
  TBL_COL_ORDER(test_any_column_order),
  TBL_OUTPUT_HUMAN_READABLE,
  TBL_COL_DELIMITER("\t"),
};

static void test_any_type(struct fastbuf *out)
{
  struct table *tbl = table_init(&test_any_tbl);

  table_start(tbl, out);

  table_col_int(tbl, TEST_ANY_COL0_INT, -10);
  table_col_int(tbl, TEST_ANY_COL1_ANY, 10000);
  table_end_row(tbl);

  table_col_int(tbl, TEST_ANY_COL0_INT, -10);
  table_col_double(tbl, TEST_ANY_COL1_ANY, 1.4);
  table_end_row(tbl);

  table_col_printf(tbl, TEST_ANY_COL0_INT, "%d", 10);
  table_col_double(tbl, TEST_ANY_COL1_ANY, 1.4);
  table_end_row(tbl);

  table_end(tbl);
  table_cleanup(tbl);
}

int main(int argc UNUSED, char **argv UNUSED)
{
  struct fastbuf *out;
  out = bfdopen_shared(1, 4096);

  test_simple1(out);

  test_any_type(out);

  bclose(out);
  return 0;
}

#endif
