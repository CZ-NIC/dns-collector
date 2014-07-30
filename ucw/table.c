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

struct table *table_init(const struct table_template *tbl_template)
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
    int cols_to_output = 0;
    for(; ; cols_to_output++) {
      if(tbl_template->column_order[cols_to_output].idx == ~0U) break;
    }

    new_inst->column_order = mp_alloc_zero(new_inst->pool, sizeof(struct table_col_instance) * cols_to_output);
    memcpy(new_inst->column_order, tbl_template->column_order, sizeof(struct table_col_instance) * cols_to_output);
    for(uint i = 0; i < new_inst->cols_to_output; i++) {
      new_inst->column_order[i].cell_content = NULL;
      int col_def_idx = new_inst->column_order[i].idx;
      new_inst->column_order[i].col_def = new_inst->columns + col_def_idx;
      new_inst->column_order[i].fmt = tbl_template->columns[col_def_idx].fmt;
    }

    new_inst->cols_to_output = cols_to_output;
  }

  new_inst->col_delimiter = tbl_template->col_delimiter;
  new_inst->print_header = true;
  new_inst->out = 0;
  new_inst->row_printing_started = false;
  new_inst->col_out = -1;
  new_inst->formatter = tbl_template->formatter;
  if(!new_inst->formatter) {
    new_inst->formatter = &table_fmt_human_readable;
  }
  new_inst->formatter_data = NULL;
  return new_inst;
}

void table_cleanup(struct table *tbl)
{
  mp_delete(tbl->pool);
}

// TODO: test default column order
static void table_make_default_column_order(struct table *tbl)
{
  struct table_col_instance *col_order = alloca(sizeof(struct table_col_instance) * (tbl->column_count + 1));
  bzero(col_order, sizeof(struct table_col_instance) * tbl->column_count);

  for(int i = 0; i < tbl->column_count; i++) {
    col_order[i].idx = (uint) i;
    // currently, XTYPE_FMT_DEFAULT is 0, so bzero actually sets it correctly. This makes it more explicit.
    col_order[i].fmt = XTYPE_FMT_DEFAULT;
  }
  struct table_col_instance tbl_col_order_end = TBL_COL_ORDER_END;
  col_order[tbl->column_count] = tbl_col_order_end;

  table_set_col_order(tbl, col_order);
}

void table_start(struct table *tbl, struct fastbuf *out)
{
  tbl->row_printing_started = false;
  tbl->out = out;

  ASSERT_MSG(tbl->out, "Output fastbuf not specified.");

  if(tbl->column_order == NULL) table_make_default_column_order(tbl);
  // update linked lists
  table_update_ll(tbl);
  if(tbl->formatter->table_start != NULL) tbl->formatter->table_start(tbl);

  mp_save(tbl->pool, &tbl->pool_state);

  ASSERT_MSG(tbl->col_delimiter, "Column delimiter not specified.");
}

void table_end(struct table *tbl)
{
  tbl->row_printing_started = false;

  mp_restore(tbl->pool, &tbl->pool_state);

  if(tbl->formatter->table_end) tbl->formatter->table_end(tbl);
}

/*** Configuration ***/

void table_set_formatter(struct table *tbl, const struct table_formatter *fmt)
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
    int col_def_idx = tbl->column_order[i].idx;
    tbl->column_order[i].col_def = tbl->columns + col_def_idx;
  }

  for(int i = 0; i < cols_to_output; i++) {
    int col_def_idx = tbl->column_order[i].idx;
    int first = tbl->ll_headers[col_def_idx];
    tbl->ll_headers[col_def_idx] = i;
    tbl->column_order[i].next_column = first;
  }
}

void table_set_col_order(struct table *tbl, const struct table_col_instance *col_order)
{
  uint cols_to_output = 0;
  for(; ; cols_to_output++) {
    if(col_order[cols_to_output].idx == ~0U) break;
    ASSERT_MSG(col_order[cols_to_output].idx < (uint) tbl->column_count,
               "Column %d does not exist; column number should be between 0 and %d(including).", col_order[cols_to_output].idx, tbl->column_count - 1);
  }

  tbl->cols_to_output = cols_to_output;
  tbl->column_order = mp_alloc(tbl->pool, sizeof(struct table_col_instance) * cols_to_output);
  memcpy(tbl->column_order, col_order, sizeof(struct table_col_instance) * cols_to_output);
  for(uint i = 0; i < cols_to_output; i++) {
    int col_def_idx = tbl->column_order[i].idx; // this is given in arg @col_order
    tbl->column_order[i].col_def = tbl->columns + col_def_idx;
    tbl->column_order[i].cell_content = NULL; // cell_content is copied from arg @col_order, so make sure that it is NULL
    tbl->column_order[i].next_column = -1;
    // tbl->column_order[i].fmt should be untouched (copied from arg @col_order)
  }
}

bool table_col_is_printed(struct table *tbl, uint col_def_idx)
{
  if(tbl->ll_headers[col_def_idx] == -1) return false;

  return true;
}

const char *table_set_col_opt(struct table *tbl, uint col_inst_idx, const char *col_opt)
{
  const struct table_column *col_def = tbl->column_order[col_inst_idx].col_def;

  // Make sure that we do not call table_set_col_opt, which would
  // result in an infinite recursion.
  if(col_def && col_def->set_col_opt) {
    ASSERT_MSG(col_def->set_col_opt != table_set_col_opt,"table_set_col_opt should not be used as a struct table_column::set_col_opt hook");
    return col_def->set_col_opt(tbl, col_inst_idx, col_opt);
  }

  if(col_def && col_def->type_def && col_def->type_def->parse_fmt) {
    uint fmt = 0;
    const char *tmp_err = col_def->type_def->parse_fmt(col_opt, &fmt, tbl->pool);
    if(tmp_err) return tmp_err;
    tbl->column_order[col_inst_idx].fmt = fmt;
    return NULL;
  }

  return mp_printf(tbl->pool, "Invalid column format option: '%s' for column %d.", col_opt, col_inst_idx);
}

/**
 * the input is a null-terminated string that contains: "<col-name>'['<param1>','<param2>\0
 * i.e., the ']' is missing and is replaced by \0.
 * the function replace the '[' by \0 and then parses the rest of the string.
 **/
static char **table_parse_col_arg2(char *col_def)
{
  char * left_br = strchr(col_def, '[');
  if(left_br == NULL) return NULL;

  *left_br = 0;
  left_br++;

  char *col_opt = left_br;

  char *next = NULL;
  char **result = NULL;
  GARY_INIT(result, 0);
  for(;;) {
    next = strchr(col_opt, ',');
    if(!next) break;
    if(*next == 0) break;
    *next = 0;
    next++;
    if(*col_opt)
      *GARY_PUSH(result) = col_opt;

    col_opt = next;
  }
  if(*col_opt)
    *GARY_PUSH(result) = col_opt;

  return result;
}

/**
 * TODO: This function deliberately leaks memory. When it is called multiple times,
 * previous column orders still remain allocated in the table's memory pool.
 **/
const char * table_set_col_order_by_name(struct table *tbl, const char *col_order_str)
{
  if(col_order_str[0] == '*') {
    table_make_default_column_order(tbl);
    return NULL;
  }

  if(!col_order_str[0]) {
    tbl->column_order = mp_alloc(tbl->pool, 0);
    tbl->cols_to_output = 0;
    return NULL;
  }

  char *tmp_col_order = stk_strdup(col_order_str);

  int col_count = 1;
  bool inside_brackets = false;
  for(int i = 0; col_order_str[i] != 0; i++) {
    if(col_order_str[i] == '[')  inside_brackets = true;
    if(col_order_str[i] == ']')  inside_brackets = false;
    if(!inside_brackets && col_order_str[i] == ',') {
      col_count++;
    }
  }

  tbl->cols_to_output = col_count;
  tbl->column_order = mp_alloc_zero(tbl->pool, sizeof(struct table_col_instance) * col_count);

  int curr_col_inst_idx = 0;
  char *name_start = tmp_col_order;
  while(name_start) {
    char *next = strpbrk(name_start, "[,");
    if(next && *next == '[') {
      next = strchr(next, ']');
      if(!next) return mp_printf(tbl->pool, "Invalid column definition, missing ']'.");
      *next++ = 0;
      next = *next == 0 ? NULL : next + 1; // if next points to the last \0 => end the computation
    } else if(next) {
      *next++ = 0;
    }

    char **args = table_parse_col_arg2(name_start); // this sets 0 on the '['
    int col_def_idx = table_get_col_idx(tbl, name_start);

    if(col_def_idx == -1) {
      return mp_printf(tbl->pool, "Unknown table column '%s', possible column names are: %s.", name_start, table_get_col_list(tbl));
    }
    tbl->column_order[curr_col_inst_idx].col_def = tbl->columns + col_def_idx;
    tbl->column_order[curr_col_inst_idx].idx = col_def_idx;
    tbl->column_order[curr_col_inst_idx].fmt = tbl->columns[col_def_idx].fmt;
    if(args) {
      for(uint i = 0; i < GARY_SIZE(args); i++) {
        const char *err = NULL;
        err = table_set_col_opt(tbl, curr_col_inst_idx, args[i]);
        if(err) return mp_printf(tbl->pool, "Error occured while setting column option: %s.", err);
      }
      GARY_FREE(args);
   }

    name_start = next;
    curr_col_inst_idx++;
  }

  return NULL;
}

/*** Table cells ***/

/**
 * The TBL_COL_ITER_START macro are used for iterating over all instances of a particular column in
 * table _tbl.  _colidx is the column index in _tbl, _instptr is the pointer to the column instance
 * (struct table_col_instance *), _idxval is the index of current column index. The variables are
 * enclosed in a block, so they do not introduce variable name collisions.
 *
 * The TBL_COL_ITER_END macro must close the block started with TBL_COL_ITER_START.
 *
 * These macros are usually used to hide the implementation details of the column instances linked
 * list. This is usefull for definition of new types.
 **/
#define TBL_COL_ITER_START(_tbl, _colidx, _instptr, _idxval) { struct table_col_instance *_instptr = NULL; int _idxval = _tbl->ll_headers[_colidx]; \
  for(_idxval = _tbl->ll_headers[_colidx], _instptr = _tbl->column_order + _idxval; _idxval != -1; _idxval = _tbl->column_order[_idxval].next_column, _instptr = _tbl->column_order + _idxval)

#define TBL_COL_ITER_END }

static void table_col_raw(struct table *tbl, int col_templ, const char *col_content)
{
  TBL_COL_ITER_START(tbl, col_templ, curr_col_ptr, curr_col) {
    curr_col_ptr->cell_content = col_content;
  } TBL_COL_ITER_END
}

void table_col_generic_format(struct table *tbl, int col, void *value, const struct xtype *expected_type)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  ASSERT(tbl->columns[col].type_def == COL_TYPE_ANY || expected_type == tbl->columns[col].type_def);
  tbl->row_printing_started = true;
  TBL_COL_ITER_START(tbl, col, curr_col, curr_col_inst_idx) {
    enum xtype_fmt fmt = curr_col->fmt;
    curr_col->cell_content = expected_type->format(value, fmt, tbl->pool);
  } TBL_COL_ITER_END
}

#undef TBL_COL_ITER_START
#undef TBL_COL_ITER_END

void table_col_printf(struct table *tbl, int col, const char *fmt, ...)
{
  ASSERT_MSG(col < tbl->column_count && col >= 0, "Table column %d does not exist.", col);
  tbl->row_printing_started = true;
  va_list args;
  va_start(args, fmt);
  char *cell_content = mp_vprintf(tbl->pool, fmt, args);
  table_col_raw(tbl, col, cell_content);
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
TABLE_COL_BODY(str, const char *)

void table_reset_row(struct table *tbl)
{
  for(uint i = 0; i < tbl->cols_to_output; i++) {
    tbl->column_order[i].cell_content = NULL;
  }
  mp_restore(tbl->pool, &tbl->pool_state);
  tbl->row_printing_started = false;
}

void table_end_row(struct table *tbl)
{
  ASSERT(tbl->formatter->row_output);
  if(tbl->row_printing_started == false) return;
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
  table_col_raw(tbl, tbl->col_out, cell_content);
  tbl->col_out = -1;
}

/*** Option parsing ***/

const char *table_set_option_value(struct table *tbl, const char *key, const char *value)
{
  // Options with no value
  if(value == NULL || (value != NULL && strlen(value) == 0)) {
    if(strcmp(key, "noheader") == 0) {
      tbl->print_header = false;
      return NULL;
    }
  }

  // Options with a value
  if(value) {
    if(strcmp(key, "header") == 0) {
      bool tmp;
      const char *err = xt_bool.parse(value, &tmp, tbl->pool);
      if(err)
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
    } else if(strcmp(key, "cells") == 0) {
      u32 fmt = 0;
      const char *err = xtype_parse_fmt(NULL, value, &fmt, tbl->pool);
      if(err) return mp_printf(tbl->pool, "Invalid cell format: '%s'.", err);
      for(uint i = 0; i < tbl->cols_to_output; i++) {
        tbl->column_order[i].fmt = fmt;
      }
      return NULL;
    } else if(strcmp(key, "raw") == 0 || strcmp(key, "pretty") == 0) {
      u32 fmt = 0;
      const char *err = xtype_parse_fmt(NULL, key, &fmt, tbl->pool);
      if(err) return mp_printf(tbl->pool, "Invalid cell format: '%s'.", err);
      for(uint i = 0; i < tbl->cols_to_output; i++) {
        tbl->column_order[i].fmt = fmt;
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
  for(uint i = 0; i < GARY_SIZE(gary_table_opts); i++) {
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

  if(tbl->print_header != false) {
    table_write_header(tbl);
  }
}

const struct table_formatter table_fmt_human_readable = {
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

  if(tbl->print_header != false && tbl->cols_to_output > 0) {
    bputs(tbl->out, tbl->column_order[0].col_def->name);
    for(uint i = 1; i < tbl->cols_to_output; i++) {
      bputs(tbl->out, tbl->col_delimiter);
      bputs(tbl->out, tbl->column_order[i].col_def->name);
    }
    bputc(tbl->out, '\n');
  }
}

const struct table_formatter table_fmt_machine_readable = {
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

const struct table_formatter table_fmt_blockline = {
  .row_output = table_row_blockline_output,
  .table_start = table_start_blockline
};

/*** Tests ***/

#ifdef TEST

#include <stdio.h>

enum test_table_cols {
  TEST_COL0_STR, TEST_COL1_INT, TEST_COL2_UINT, TEST_COL3_BOOL, TEST_COL4_DOUBLE
};

static struct table_col_instance test_column_order[] = { TBL_COL(TEST_COL3_BOOL), TBL_COL(TEST_COL4_DOUBLE),
                      TBL_COL(TEST_COL2_UINT), TBL_COL(TEST_COL1_INT), TBL_COL(TEST_COL0_STR), TBL_COL_ORDER_END };

static struct table_template test_tbl = {
  TBL_COLUMNS {
    [TEST_COL0_STR] = TBL_COL_STR("col0_str", 20),
    [TEST_COL1_INT] = TBL_COL_INT("col1_int", 8),
    [TEST_COL2_UINT] = TBL_COL_UINT("col2_uint", 9),
    [TEST_COL3_BOOL] = TBL_COL_BOOL_FMT("col3_bool", 9, XTYPE_FMT_PRETTY),
    [TEST_COL4_DOUBLE] = TBL_COL_DOUBLE("col4_double", 11),
    TBL_COL_END
  },
  TBL_COL_ORDER(test_column_order),
  TBL_FMT_HUMAN_READABLE,
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
  tbl->print_header = false;
  table_start(tbl, out);
  do_print1(tbl);
  table_end(tbl);
  tbl->print_header = true;

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


  // test table_col_order_fmt
  struct table_col_instance col_order[] = { TBL_COL(TEST_COL0_STR), TBL_COL_FMT(TEST_COL4_DOUBLE, XTYPE_FMT_PRETTY), TBL_COL_FMT(TEST_COL4_DOUBLE, XTYPE_FMT_RAW), TBL_COL_ORDER_END };
  table_set_col_order(tbl, col_order);
  table_start(tbl, out);

  table_col_str(tbl, TEST_COL0_STR, "test");
  table_col_double(tbl, TEST_COL4_DOUBLE, 1.23456789);
  table_end_row(tbl);

  table_col_str(tbl, TEST_COL0_STR, "test");
  table_col_double(tbl, TEST_COL4_DOUBLE, 1.23456789);
  table_end_row(tbl);

  table_end(tbl);

  table_cleanup(tbl);
}

enum test_any_table_cols {
  TEST_ANY_COL0_INT, TEST_ANY_COL1_ANY
};

static struct table_col_instance test_any_column_order[] = { TBL_COL(TEST_ANY_COL0_INT), TBL_COL_FMT(TEST_ANY_COL1_ANY, XTYPE_FMT_PRETTY), TBL_COL_ORDER_END };

static struct table_template test_any_tbl = {
  TBL_COLUMNS {
    [TEST_ANY_COL0_INT] = TBL_COL_INT("col0_int", 8),
    [TEST_ANY_COL1_ANY] = TBL_COL_ANY_FMT("col1_any", 9, XTYPE_FMT_PRETTY),
    TBL_COL_END
  },
  TBL_COL_ORDER(test_any_column_order),
  TBL_FMT_HUMAN_READABLE,
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
