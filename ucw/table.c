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

#include <stdlib.h>

/*** Management of tables ***/

void table_init(struct table *tbl, struct fastbuf *out)
{
  tbl->out = out;

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
    ASSERT(tbl->columns[col_count].type < COL_TYPE_LAST);
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
  table_col_order(tbl, col_order_int, tbl->column_count);
}

void table_start(struct table *tbl)
{
  tbl->last_printed_col = -1;
  tbl->row_printing_started = 0;

  if(!tbl->col_str_ptrs) {
    tbl->col_str_ptrs = mp_alloc_zero(tbl->pool, sizeof(char *) * tbl->column_count);
  }

  if(tbl->column_order == NULL) table_make_default_column_order(tbl);

  if(tbl->formatter->table_start != NULL) tbl->formatter->table_start(tbl);

  mp_save(tbl->pool, &tbl->pool_state);

  ASSERT_MSG(tbl->col_delimiter, "In-between column delimiter not specified.");
  ASSERT_MSG(tbl->append_delimiter, "Append delimiter not specified.");
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

// FIXME: Shouldn't this be table_SET_col_order() ?
void table_col_order(struct table *tbl, int *col_order, int cols_to_output)
{
  for(int i = 0; i < cols_to_output; i++) {
    ASSERT_MSG(col_order[i] >= 0 && col_order[i] < tbl->column_count, "Column %d does not exist (column number should be between 0 and %d)", col_order[i], tbl->column_count - 1);
  }

  tbl->column_order = col_order;
  tbl->cols_to_output = cols_to_output;
}

/**
 * TODO: This function deliberately leaks memory. When it is called multiple times,
 * previous column orders still remain allocated in the table's memory pool.
 **/
const char * table_col_order_by_name(struct table *tbl, const char *col_order_str)
{
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

  int *col_order_int = mp_alloc_zero(tbl->pool, sizeof(int) * col_count);
  int curr_col_order_int = 0;
  const char *name_start = tmp_col_order;
  while(name_start) {
    char *next = strchr(name_start, ',');
    if(next) {
      *next++ = 0;
    }

    int idx = table_get_col_idx(tbl, name_start);
    if(idx == -1) {
      return mp_printf(tbl->pool, "Unknown table column '%s'", name_start);
    }
    col_order_int[curr_col_order_int++] = idx;

    name_start = next;
  }

  tbl->column_order = col_order_int;
  tbl->cols_to_output = curr_col_order_int;
  return NULL;
}

/*** Table cells ***/

void table_set_printf(struct table *tbl, int col, const char *fmt, ...)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;
  va_list args;
  va_start(args, fmt);
  tbl->col_str_ptrs[col] = mp_vprintf(tbl->pool, fmt, args);
  va_end(args);
}

static const char *table_set_col_default_fmts[] = {
  [COL_TYPE_STR] = "%s",
  [COL_TYPE_INT] = "%d",
  [COL_TYPE_INTMAX] = "%jd",
  [COL_TYPE_UINT] = "%u",
  [COL_TYPE_UINTMAX] = "%ju",
  [COL_TYPE_BOOL] = "%d",
  [COL_TYPE_DOUBLE] = "%.2lf",
  [COL_TYPE_ANY] = NULL,
  [COL_TYPE_LAST] = NULL
};

#define TABLE_SET_COL(_name_, _type_, _typeconst_) void table_set_##_name_(struct table *tbl, int col, _type_ val) \
  {\
    const char *fmt = tbl->columns[col].fmt;\
    if(tbl->columns[col].type == COL_TYPE_ANY) {\
       fmt = table_set_col_default_fmts[_typeconst_];\
    }\
    table_set_##_name_##_fmt(tbl, col, fmt, val);\
  }

#define TABLE_SET_COL_STR(_name_, _type_, _typeconst_) void table_set_##_name_##_name(struct table *tbl, const char *col_name, _type_ val) \
  {\
    int col = table_get_col_idx(tbl, col_name);\
    table_set_##_name_(tbl, col, val);\
  }

#define TABLE_SET_COL_FMT(_name_, _type_, _typeconst_) void table_set_##_name_##_fmt(struct table *tbl, int col, const char *fmt, _type_ val)\
  {\
     ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);\
     ASSERT(tbl->columns[col].type == COL_TYPE_ANY || _typeconst_ == tbl->columns[col].type);\
     ASSERT(fmt != NULL);\
     tbl->last_printed_col = col;\
     tbl->row_printing_started = 1;\
     tbl->col_str_ptrs[col] = mp_printf(tbl->pool, fmt, val);\
  }

#define TABLE_SET(_name_, _type_, _typeconst_) TABLE_SET_COL(_name_, _type_, _typeconst_);\
  TABLE_SET_COL_STR(_name_, _type_, _typeconst_);\
  TABLE_SET_COL_FMT(_name_, _type_, _typeconst_);

TABLE_SET(int, int, COL_TYPE_INT)
TABLE_SET(uint, uint, COL_TYPE_UINT)
TABLE_SET(double, double, COL_TYPE_DOUBLE)
TABLE_SET(str, const char *, COL_TYPE_STR)
TABLE_SET(intmax, intmax_t, COL_TYPE_INTMAX)
TABLE_SET(uintmax, uintmax_t, COL_TYPE_UINTMAX)
#undef TABLE_SET_COL_FMT
#undef TABLE_SET_COL_STR
#undef TABLE_SET_COL
#undef TABLE_SET

void table_set_bool(struct table *tbl, int col, uint val)
{
  table_set_bool_fmt(tbl, col, tbl->columns[col].fmt, val);
}

void table_set_bool_name(struct table *tbl, const char *col_name, uint val)
{
  int col = table_get_col_idx(tbl, col_name);
  table_set_bool(tbl, col, val);
}

void table_set_bool_fmt(struct table *tbl, int col, const char *fmt, uint val)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(COL_TYPE_BOOL == tbl->columns[col].type);

  tbl->last_printed_col = col;
  tbl->row_printing_started = 1;
  tbl->col_str_ptrs[col] = mp_printf(tbl->pool, fmt, val ? "true" : "false");
}

#define TABLE_APPEND(_name_, _type_, _typeconst_) void table_append_##_name_(struct table *tbl, _type_ val) \
  {\
     ASSERT(tbl->last_printed_col != -1 || tbl->row_printing_started != 0);\
     ASSERT(_typeconst_ == tbl->columns[tbl->last_printed_col].type);\
     int col = tbl->last_printed_col;\
     mp_printf_append(tbl->pool, tbl->col_str_ptrs[col], "%s", tbl->append_delimiter);\
     tbl->col_str_ptrs[col] = mp_printf_append(tbl->pool, tbl->col_str_ptrs[col], tbl->columns[col].fmt, val);\
  }

TABLE_APPEND(int, int, COL_TYPE_INT)
TABLE_APPEND(uint, uint, COL_TYPE_UINT)
TABLE_APPEND(double, double, COL_TYPE_DOUBLE)
TABLE_APPEND(str, const char *, COL_TYPE_STR)
TABLE_APPEND(intmax, intmax_t, COL_TYPE_INTMAX)
TABLE_APPEND(uintmax, uintmax_t, COL_TYPE_UINTMAX)
#undef TABLE_APPEND

void table_append_bool(struct table *tbl, int val)
{
  ASSERT(tbl->last_printed_col != -1 || tbl->row_printing_started != 0);
  ASSERT(COL_TYPE_BOOL == tbl->columns[tbl->last_printed_col].type);

  int col = tbl->last_printed_col;

  mp_printf_append(tbl->pool, tbl->col_str_ptrs[col], "%s", tbl->append_delimiter);

  tbl->col_str_ptrs[col] = mp_printf_append(tbl->pool, tbl->col_str_ptrs[col], tbl->columns[col].fmt, val ? "true" : "false");
}

void table_append_printf(struct table *tbl, const char *fmt, ...)
{
  ASSERT(tbl->last_printed_col != -1 || tbl->row_printing_started != 0);
  int col = tbl->last_printed_col;

  va_list args;
  va_start(args, fmt);

  mp_printf_append(tbl->pool, tbl->col_str_ptrs[col], "%s", tbl->append_delimiter);
  tbl->col_str_ptrs[col] = mp_vprintf_append(tbl->pool, tbl->col_str_ptrs[col], fmt, args);

  va_end(args);
}

void table_end_row(struct table *tbl)
{
  ASSERT(tbl->formatter->row_output);
  tbl->formatter->row_output(tbl);
  memset(tbl->col_str_ptrs, 0, sizeof(char *) * tbl->column_count);
  mp_restore(tbl->pool, &tbl->pool_state);
  tbl->last_printed_col = -1;
  tbl->row_printing_started = 0;
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
  tbl->col_str_ptrs[tbl->col_out] = fbpool_end(&tbl->fb_col_out);
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
      // FIXME: Check syntax of value.
      //tbl->print_header = strtol(value, NULL, 10); //atoi(value);
      //if(errno != 0) tbl->print_header
      if(value[1] != 0)
        return mp_printf(tbl->pool, "Tableprinter: invalid option: '%s' has invalid value: '%s'.", key, value);
      uint tmp = value[0] - '0';
      if(tmp > 1)
        return mp_printf(tbl->pool, "Tableprinter: invalid option: '%s' has invalid value: '%s'.", key, value);
      tbl->print_header = tmp;
      return NULL;
    } else if(strcmp(key, "cols") == 0) {
      const char *err = table_col_order_by_name(tbl, value);
      if(err != NULL) {
        return mp_printf(tbl->pool, "%s, possible column names are: %s.", err, table_get_col_list(tbl));
      }
      return NULL;
    } else if(strcmp(key, "fmt") == 0) {
      if(strcmp(value, "human") == 0) table_set_formatter(tbl, &table_fmt_human_readable);
      else if(strcmp(value, "machine") == 0) table_set_formatter(tbl, &table_fmt_machine_readable);
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
    if (tbl->formatter->process_option(tbl, key, value, &err)) {
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
    int col_idx = tbl->column_order[i];
    int col_width = tbl->columns[col_idx].width;
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    bprintf(tbl->out, "%*s", col_width, tbl->col_str_ptrs[col_idx]);
  }
  bputc(tbl->out, '\n');
}

static void table_write_header(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    int col_idx = tbl->column_order[i];
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    bprintf(tbl->out, "%*s", tbl->columns[col_idx].width, tbl->columns[col_idx].name);
  }
  bputc(tbl->out, '\n');
}

static void table_start_human_readable(struct table *tbl)
{
  if(tbl->col_delimiter == NULL) {
    tbl->col_delimiter = " ";
  }

  if(tbl->append_delimiter == NULL) {
    tbl->append_delimiter = ",";
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
    int col_idx = tbl->column_order[i];
    if(i) {
      bputs(tbl->out, tbl->col_delimiter);
    }
    bputs(tbl->out, tbl->col_str_ptrs[col_idx]);
  }
  bputc(tbl->out, '\n');
}

static void table_start_machine_readable(struct table *tbl)
{
  if(tbl->col_delimiter == NULL) {
    tbl->col_delimiter = ";";
  }

  if(tbl->append_delimiter == NULL) {
    tbl->append_delimiter = ",";
  }

  if(tbl->print_header != 0) {
    uint col_idx = tbl->column_order[0];
    bputs(tbl->out, tbl->columns[col_idx].name);
    for(uint i = 1; i < tbl->cols_to_output; i++) {
      col_idx = tbl->column_order[i];
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

/*** Tests ***/

#ifdef TEST

#include <stdio.h>

enum test_table_cols {
  test_col0_str, test_col1_int, test_col2_uint, test_col3_bool, test_col4_double
};

static uint test_column_order[] = {test_col3_bool, test_col4_double, test_col2_uint,test_col1_int, test_col0_str};

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
  TBL_APPEND_DELIMITER(",")
};

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
  table_set_printf(test_tbl, test_col4_double, "AAA");
  table_end_row(test_tbl);

  table_set_str(test_tbl, test_col0_str, "test");
  table_append_str(test_tbl, "bbbbb");
  table_set_int(test_tbl, test_col1_int, -100);
  table_set_uint(test_tbl, test_col2_uint, 100);
  table_set_bool(test_tbl, test_col3_bool, 0);
  table_set_printf(test_tbl, test_col4_double, "%.2lf", 1.5);
  table_end_row(test_tbl);
}

static void test_simple1(struct fastbuf *out)
{
  table_init(&test_tbl, out);
  // print table with header
  table_col_order_by_name(&test_tbl, "col3_bool");
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  // print the same table as in the previous case without header
  table_col_order_by_name(&test_tbl, "col0_str,col2_uint,col1_int,col3_bool");
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  // this also tests whether there is need to call table_col_order_by_name after table_end was called
  test_tbl.print_header = 0;
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);
  test_tbl.print_header = 1;

  table_col_order_by_name(&test_tbl, "col3_bool");
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_col_order_by_name(&test_tbl, "col3_bool,col0_str");
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_col_order_by_name(&test_tbl, "col0_str,col3_bool,col2_uint");
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_col_order_by_name(&test_tbl, "col0_str,col3_bool,col2_uint,col0_str,col3_bool,col2_uint,col0_str,col3_bool,col2_uint");
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_col_order_by_name(&test_tbl, "col0_str,col1_int,col2_uint,col3_bool,col4_double");
  table_start(&test_tbl);
  do_print1(&test_tbl);
  table_end(&test_tbl);

  table_cleanup(&test_tbl);
}

enum test_any_table_cols {
  test_any_col0_int, test_any_col1_any
};

static uint test_any_column_order[] = { test_any_col0_int, test_any_col1_any };

static struct table test_any_tbl = {
  TBL_COLUMNS {
    TBL_COL_INT(test_any, col0_int, 8),
    TBL_COL_ANY(test_any, col1_any, 9),
    TBL_COL_END
  },
  TBL_COL_ORDER(test_any_column_order),
  TBL_OUTPUT_HUMAN_READABLE,
  TBL_COL_DELIMITER("\t"),
  TBL_APPEND_DELIMITER(",")
};

static void test_any_type(struct fastbuf *out)
{
  table_init(&test_any_tbl, out);
  table_start(&test_any_tbl);

  table_set_int(&test_any_tbl, test_any_col0_int, -10);
  table_set_int(&test_any_tbl, test_any_col1_any, 10000);
  table_end_row(&test_any_tbl);

  table_set_int(&test_any_tbl, test_any_col0_int, -10);
  table_set_double(&test_any_tbl, test_any_col1_any, 1.4);
  table_end_row(&test_any_tbl);

  table_set_printf(&test_any_tbl, test_any_col0_int, "%d", 10);
  table_append_printf(&test_any_tbl, "%d", 20);
  table_append_printf(&test_any_tbl, "%d", 30);
  table_set_double(&test_any_tbl, test_any_col1_any, 1.4);
  table_append_printf(&test_any_tbl, "%.2lf", 1.5);
  table_append_printf(&test_any_tbl, "%.2lf", 1.6);
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
