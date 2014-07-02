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

void table_init(struct table *tbl)
{
  int col_count = 0; // count the number of columns in the struct table

  for(;;) {
    if(tbl->columns[col_count].name == NULL &&
       tbl->columns[col_count].fmt == NULL &&
       tbl->columns[col_count].width == 0 &&
       tbl->columns[col_count].type == COL_TYPE_LAST)
      break;
    ASSERT(tbl->columns[col_count].name != NULL);
    ASSERT(tbl->columns[col_count].type == COL_TYPE_ANY || tbl->columns[col_count].fmt != NULL);
    ASSERT(tbl->columns[col_count].width != 0);

    col_count++;
  }
  tbl->pool = mp_new(4096);

  tbl->column_count = col_count;

  if(!tbl->formatter) {
    tbl->formatter = &table_fmt_human_readable;
  }

  tbl->print_header = 1; // by default, print header
}

void table_cleanup(struct table *tbl)
{
  mp_delete(tbl->pool);
  memset(tbl, 0, sizeof(struct table));
}

// TODO: test default column order
static void table_make_default_column_order(struct table *tbl)
{
  int *col_order_int = mp_alloc_zero(tbl->pool, sizeof(int) * tbl->column_count);
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
    tbl->columns[i].first_column = -1;
    tbl->columns[i].last_column = -1;
  }

  for(int i = 0; i < cols_to_output; i++) {
    int col_idx = tbl->column_order[i].idx;
    int last = tbl->columns[col_idx].last_column;
    if(last != -1) {
      tbl->columns[col_idx].last_column = i;
      tbl->column_order[last].next_column = i;
    } else {
      tbl->columns[col_idx].last_column = i;
      tbl->columns[col_idx].first_column = i;
    }
    tbl->column_order[i].next_column = -1;
  }
}

void table_set_col_order(struct table *tbl, int *col_order, int cols_to_output)
{
  for(int i = 0; i < cols_to_output; i++) {
    ASSERT_MSG(col_order[i] >= 0 && col_order[i] < tbl->column_count, "Column %d does not exist (column number should be between 0 and %d)", col_order[i], tbl->column_count - 1);
  }

  tbl->cols_to_output = cols_to_output;
  tbl->column_order = mp_alloc_zero(tbl->pool, sizeof(struct table_col_info) * cols_to_output);
  for(int i = 0; i < cols_to_output; i++) {
    int col_idx = col_order[i];
    tbl->column_order[i].idx = col_idx;
    tbl->column_order[i].cell_content = NULL;
    tbl->column_order[i].output_type = CELL_OUT_UNINITIALIZED;
  }
  table_update_ll(tbl);
}

bool table_col_is_printed(struct table *tbl, uint col_idx)
{
  if(tbl->columns[col_idx].first_column == -1) return 0;

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
  //*left_br = 0;
  return left_br;
}

/**
 * Setting options for basic table types (as defined in table.h)
 **/
bool table_set_col_opt_default(struct table *tbl, int col_copy_idx, const char *col_arg, char **err)
{
  int col_type_idx = tbl->column_order[col_copy_idx].idx;

  if(tbl->columns[col_type_idx].type == COL_TYPE_DOUBLE) {
    uint precision = 0;
    str_to_uint(&precision, col_arg, NULL, 0);
    tbl->column_order[col_type_idx].output_type = precision;
    return true;
  }

  *err = mp_printf(tbl->pool, "Tableprinter: invalid column format option: '%s' for column %d.", col_arg, col_copy_idx);
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
  tbl->column_order = mp_alloc_zero(tbl->pool, sizeof(struct table_col_info) * col_count);

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
    tbl->column_order[curr_col_idx].idx = col_idx;
    tbl->column_order[curr_col_idx].cell_content = NULL;
    tbl->column_order[curr_col_idx].output_type = CELL_OUT_UNINITIALIZED;
    if(tbl->columns[col_idx].type_def && tbl->columns[col_idx].type_def->set_col_instance_option) {
      char *err = NULL;
      tbl->columns[col_idx].type_def->set_col_instance_option(tbl, curr_col_idx, arg, &err);
      if(err) return mp_printf(tbl->pool, "Error occured while setting column option: %s.", err);
    }

    name_start = next;
    curr_col_idx++;
  }

  table_update_ll(tbl);

  return NULL;
}

/*** Table cells ***/

static void table_set_all_cols_content(struct table *tbl, int col, char *col_content, int override)
{
  int curr_col = tbl->columns[col].first_column;
  while(curr_col != -1) {
    if(override == 0 && tbl->column_order[curr_col].output_type != CELL_OUT_UNINITIALIZED ) {
      die("Error while setting content of all cells of a single type column, cell format should not be overriden.");
    }
    tbl->column_order[curr_col].cell_content = col_content;
    curr_col = tbl->column_order[curr_col].next_column;
  }
}

void table_col_printf(struct table *tbl, int col, const char *fmt, ...)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;
  va_list args;
  va_start(args, fmt);
  char *cell_content = mp_vprintf(tbl->pool, fmt, args);
  table_set_all_cols_content(tbl, col, cell_content, 1);
  va_end(args);
}

static const char *table_col_default_fmts[] = {
  [COL_TYPE_STR] = "%s",
  [COL_TYPE_INT] = "%d",
  [COL_TYPE_S64] = "%lld",
  [COL_TYPE_INTMAX] = "%jd",
  [COL_TYPE_UINT] = "%u",
  [COL_TYPE_U64] = "%llu",
  [COL_TYPE_UINTMAX] = "%ju",
  [COL_TYPE_BOOL] = "%d",
  [COL_TYPE_DOUBLE] = "%.2lf",
  [COL_TYPE_ANY] = NULL,
  [COL_TYPE_LAST] = NULL
};

#define TABLE_COL(_name_, _type_, _typeconst_) void table_col_##_name_(struct table *tbl, int col, _type_ val)\
  {\
    const char *fmt = tbl->columns[col].fmt;\
    if(tbl->columns[col].type == COL_TYPE_ANY) {\
       fmt = table_col_default_fmts[_typeconst_];\
    }\
    table_col_##_name_##_fmt(tbl, col, fmt, val);\
  }

#define TABLE_COL_STR(_name_, _type_, _typeconst_) void table_col_##_name_##_name(struct table *tbl, const char *col_name, _type_ val)\
  {\
    int col = table_get_col_idx(tbl, col_name);\
    table_col_##_name_(tbl, col, val);\
  }

#define TABLE_COL_FMT(_name_, _type_, _typeconst_) void table_col_##_name_##_fmt(struct table *tbl, int col, const char *fmt, _type_ val)\
  {\
     ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);\
     ASSERT(tbl->columns[col].type == COL_TYPE_ANY || _typeconst_ == tbl->columns[col].type);\
     ASSERT(fmt != NULL);\
     tbl->last_printed_col = col;\
     tbl->row_printing_started = 1;\
     char *cell_content = mp_printf(tbl->pool, fmt, val);\
     table_set_all_cols_content(tbl, col, cell_content, 0);\
  }

#define TABLE_COL_BODIES(_name_, _type_, _typeconst_) TABLE_COL(_name_, _type_, _typeconst_);\
  TABLE_COL_STR(_name_, _type_, _typeconst_);\
  TABLE_COL_FMT(_name_, _type_, _typeconst_);

TABLE_COL_BODIES(int, int, COL_TYPE_INT)
TABLE_COL_BODIES(uint, uint, COL_TYPE_UINT)
TABLE_COL_BODIES(str, const char *, COL_TYPE_STR)
TABLE_COL_BODIES(intmax, intmax_t, COL_TYPE_INTMAX)
TABLE_COL_BODIES(uintmax, uintmax_t, COL_TYPE_UINTMAX)
TABLE_COL_BODIES(s64, s64, COL_TYPE_S64)
TABLE_COL_BODIES(u64, u64, COL_TYPE_U64)

// column type double is a special case
TABLE_COL(double, double, COL_TYPE_DOUBLE);
TABLE_COL_STR(double, double, COL_TYPE_DOUBLE);
#undef TABLE_COL
#undef TABLE_COL_FMT
#undef TABLE_COL_STR
#undef TABLE_COL_BODIES

void table_col_double_fmt(struct table *tbl, int col, const char *fmt, double val)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(tbl->columns[col].type == COL_TYPE_ANY || COL_TYPE_DOUBLE == tbl->columns[col].type);
  ASSERT(fmt != NULL);
  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;
  char *cell_content = mp_printf(tbl->pool, fmt, val);
  int curr_col = tbl->columns[col].first_column;
  while(curr_col != -1) {
    if(tbl->column_order[curr_col].output_type < 0) tbl->column_order[curr_col].cell_content = cell_content;
    else {
      char *cell_content_tmp = mp_printf(tbl->pool, "%.*lf", tbl->column_order[curr_col].output_type, val);
      tbl->column_order[curr_col].cell_content = cell_content_tmp;
    }
    curr_col = tbl->column_order[curr_col].next_column;
  }
}

void table_col_bool(struct table *tbl, int col, uint val)
{
  table_col_bool_fmt(tbl, col, tbl->columns[col].fmt, val);
}

void table_col_bool_name(struct table *tbl, const char *col_name, uint val)
{
  int col = table_get_col_idx(tbl, col_name);
  table_col_bool(tbl, col, val);
}

void table_col_bool_fmt(struct table *tbl, int col, const char *fmt, uint val)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(COL_TYPE_BOOL == tbl->columns[col].type);

  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;

  int curr_col = tbl->columns[col].first_column;
  while(curr_col != -1) {
    switch(tbl->column_order[curr_col].output_type) {
    case CELL_OUT_HUMAN_READABLE:
    case CELL_OUT_UNINITIALIZED:
      tbl->column_order[curr_col].cell_content = mp_printf(tbl->pool, fmt, val ? "true" : "false");
      break;
    case CELL_OUT_MACHINE_READABLE:
      // FIXME: this is just an example of printing in different formats
      tbl->column_order[curr_col].cell_content = mp_printf(tbl->pool, fmt, val ? "1" : "0");
      break;
    default:
      die("Unsupported output type.");
    }
    curr_col = tbl->column_order[curr_col].next_column;
  }

  // FIXME: add to printing of all columns
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
  table_set_all_cols_content(tbl, tbl->col_out, cell_content, 1);
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
        return mp_printf(tbl->pool, "Tableprinter: invalid option: '%s' has invalid value: '%s'.", key, value);
      uint tmp = value[0] - '0';
      if(tmp > 1)
        return mp_printf(tbl->pool, "Tableprinter: invalid option: '%s' has invalid value: '%s'.", key, value);
      tbl->print_header = tmp;
      return NULL;
    } else if(strcmp(key, "cols") == 0) {
      const char *err = table_set_col_order_by_name(tbl, value);
      if(err != NULL) {
        return err;
      }
      return NULL;
    } else if(strcmp(key, "fmt") == 0) {
      if(strcmp(value, "human") == 0) table_set_formatter(tbl, &table_fmt_human_readable);
      else if(strcmp(value, "machine") == 0) table_set_formatter(tbl, &table_fmt_machine_readable);
      else if(strcmp(value, "blockline") == 0) table_set_formatter(tbl, &table_fmt_blockline);
      else {
        return "Tableprinter: invalid argument to output-type option.";
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
  return mp_printf(tbl->pool, "Tableprinter: invalid option: '%s%s%s'.", key, (value ? ":" : ""), (value ? : ""));
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
    int col_idx = tbl->column_order[i].idx;
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    int col_width = tbl->columns[col_idx].width & CELL_WIDTH_MASK;
    if(tbl->columns[col_idx].width & CELL_ALIGN_LEFT) col_width = -1 * col_width;
    bprintf(tbl->out, "%*s", col_width, tbl->column_order[i].cell_content);
  }
  bputc(tbl->out, '\n');
}

static void table_write_header(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    int col_idx = tbl->column_order[i].idx;
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    int col_width = tbl->columns[col_idx].width & CELL_WIDTH_MASK;
    if(tbl->columns[col_idx].width & CELL_ALIGN_LEFT) col_width = -1 * col_width;
    bprintf(tbl->out, "%*s", col_width, tbl->columns[col_idx].name);
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

  if(tbl->print_header != 0) {
    uint col_idx = tbl->column_order[0].idx;
    bputs(tbl->out, tbl->columns[col_idx].name);
    for(uint i = 1; i < tbl->cols_to_output; i++) {
      col_idx = tbl->column_order[i].idx;
      bputs(tbl->out, tbl->col_delimiter);
      bputs(tbl->out, tbl->columns[col_idx].name);
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
    int col_idx = tbl->column_order[i].idx;
    bprintf(tbl->out, "%s: %s\n", tbl->columns[col_idx].name, tbl->column_order[i].cell_content);
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
  test_col0_str, test_col1_int, test_col2_uint, test_col3_bool, test_col4_double
};

static struct table_col_info test_column_order[] = { TBL_COL(test_col3_bool), TBL_COL(test_col4_double), TBL_COL(test_col2_uint), TBL_COL(test_col1_int), TBL_COL(test_col0_str) };

static struct table test_tbl = {
  TBL_COLUMNS {
    [test_col0_str] = TBL_COL_STR("col0_str", 20),
    [test_col1_int] = TBL_COL_INT("col1_int", 8),
    [test_col2_uint] = TBL_COL_UINT("col2_uint", 9),
    [test_col3_bool] = TBL_COL_BOOL("col3_bool", 9),
    [test_col4_double] = TBL_COL_DOUBLE("col4_double", 11, 2),
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
  table_col_str(test_tbl, test_col0_str, "sdsdf");
  table_col_int(test_tbl, test_col1_int, -10);
  table_col_int(test_tbl, test_col1_int, 10000);
  table_col_uint(test_tbl, test_col2_uint, 10);
  table_col_printf(test_tbl, test_col2_uint, "XXX-%u", 22222);
  table_col_bool(test_tbl, test_col3_bool, 1);
  table_col_double(test_tbl, test_col4_double, 1.5);
  table_col_printf(test_tbl, test_col4_double, "AAA");
  table_end_row(test_tbl);

  table_col_str(test_tbl, test_col0_str, "test");
  table_col_int(test_tbl, test_col1_int, -100);
  table_col_uint(test_tbl, test_col2_uint, 100);
  table_col_bool(test_tbl, test_col3_bool, 0);
  table_col_printf(test_tbl, test_col4_double, "%.2lf", 1.5);
  table_end_row(test_tbl);
}

static void test_simple1(struct fastbuf *out)
{
  table_init(&test_tbl);

  // print table with header
  table_set_col_order_by_name(&test_tbl, "col3_bool");
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  // print the same table as in the previous case without header
  table_set_col_order_by_name(&test_tbl, "col0_str,col2_uint,col1_int,col3_bool");
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  // this also tests whether there is need to call table_set_col_order_by_name after table_end was called
  test_tbl.print_header = 0;
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);
  test_tbl.print_header = 1;

  table_set_col_order_by_name(&test_tbl, "col3_bool");
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_set_col_order_by_name(&test_tbl, "col3_bool,col0_str");
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_set_col_order_by_name(&test_tbl, "col0_str,col3_bool,col2_uint");
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_set_col_order_by_name(&test_tbl, "col0_str,col3_bool,col2_uint,col0_str,col3_bool,col2_uint,col0_str,col3_bool,col2_uint");
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_set_col_order_by_name(&test_tbl, "col0_str,col1_int,col2_uint,col3_bool,col4_double");
  table_start(&test_tbl, out);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_cleanup(&test_tbl);
}

enum test_any_table_cols {
  test_any_col0_int, test_any_col1_any
};

static struct table_col_info test_any_column_order[] = { TBL_COL(test_any_col0_int), TBL_COL(test_any_col1_any) };

static struct table test_any_tbl = {
  TBL_COLUMNS {
    [test_any_col0_int] = TBL_COL_INT("col0_int", 8),
    [test_any_col1_any] = TBL_COL_ANY("col1_any", 9),
    TBL_COL_END
  },
  TBL_COL_ORDER(test_any_column_order),
  TBL_OUTPUT_HUMAN_READABLE,
  TBL_COL_DELIMITER("\t"),
};

static void test_any_type(struct fastbuf *out)
{
  table_init(&test_any_tbl);

  table_start(&test_any_tbl, out);

  table_col_int(&test_any_tbl, test_any_col0_int, -10);
  table_col_int(&test_any_tbl, test_any_col1_any, 10000);
  table_end_row(&test_any_tbl);

  table_col_int(&test_any_tbl, test_any_col0_int, -10);
  table_col_double(&test_any_tbl, test_any_col1_any, 1.4);
  table_end_row(&test_any_tbl);

  table_col_printf(&test_any_tbl, test_any_col0_int, "%d", 10);
  table_col_double(&test_any_tbl, test_any_col1_any, 1.4);
  table_end_row(&test_any_tbl);

  table_end(&test_any_tbl);
  table_cleanup(&test_any_tbl);
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
