/*
 *	UCW Library -- Table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#ifndef _TABLEPRINTER_H
#define _TABLEPRINTER_H

#include <ucw/fastbuf.h>
#include <ucw/mempool.h>

enum column_type {
  COL_TYPE_STR, COL_TYPE_INT, COL_TYPE_INTMAX, COL_TYPE_UINT, COL_TYPE_UINTMAX, COL_TYPE_BOOL, COL_TYPE_DOUBLE, COL_TYPE_ANY, COL_TYPE_LAST
};

#define TBL_COL_STR(_enum_prefix, _name, _width)            [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "%s", .type = COL_TYPE_STR }
#define TBL_COL_INT(_enum_prefix, _name, _width)            [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "%d", .type = COL_TYPE_INT }
#define TBL_COL_UINT(_enum_prefix, _name, _width)           [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "%u", .type = COL_TYPE_UINT }
#define TBL_COL_INTMAX(_enum_prefix, _name, _width)         [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "%jd", .type = COL_TYPE_INTMAX }
#define TBL_COL_UINTMAX(_enum_prefix, _name, _width)        [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "%ju", .type = COL_TYPE_UINTMAX }
#define TBL_COL_HEXUINT(_enum_prefix, _name, _width)        [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "0x%x", .type = COL_TYPE_UINT }
#define TBL_COL_DOUBLE(_enum_prefix, _name, _width, _prec)  [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "%." #_prec "lf", .type = COL_TYPE_DOUBLE }
#define TBL_COL_BOOL(_enum_prefix, _name, _width)           [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = "%s", .type = COL_TYPE_BOOL }
#define TBL_COL_ANY(_enum_prefix, _name, _width)            [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = 0, .type = COL_TYPE_ANY }

#define TBL_COL_STR_FMT(_enum_prefix, _name, _width, _fmt)            [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = _fmt, .type = COL_TYPE_STR }
#define TBL_COL_INT_FMT(_enum_prefix, _name, _width, _fmt)            [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = _fmt, .type = COL_TYPE_INT }
#define TBL_COL_UINT_FMT(_enum_prefix, _name, _width, _fmt)           [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = _fmt, .type = COL_TYPE_UINT }
#define TBL_COL_INTMAX_FMT(_enum_prefix, _name, _width, _fmt)         [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = _fmt, .type = COL_TYPE_INTMAX }
#define TBL_COL_UINTMAX_FMT(_enum_prefix, _name, _width, _fmt)        [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = _fmt, .type = COL_TYPE_UINTMAX }
#define TBL_COL_HEXUINT_FMT(_enum_prefix, _name, _width, _fmt)        [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = _fmt, .type = COL_TYPE_UINT }
#define TBL_COL_BOOL_FMT(_enum_prefix, _name, _width, _fmt)           [_enum_prefix##_##_name] = { .name = #_name, .width = _width, .fmt = _fmt, .type = COL_TYPE_BOOL }

#define TBL_COL_END { .name = 0, .width = 0, .fmt = 0, .type = COL_TYPE_LAST }

#define TBL_COLUMNS  .columns = (struct table_column [])
#define TBL_COL_ORDER(order) .column_order = (int *) order, .cols_to_output = ARRAY_SIZE(order)
#define TBL_COL_DELIMITER(_delimiter_) .col_delimiter = _delimiter_
#define TBL_APPEND_DELIMITER(_delimiter_) .append_delimiter = _delimiter_

#define TBL_OUTPUT_HUMAN_READABLE     .callbacks = &table_fmt_human_readable
#define TBL_OUTPUT_MACHINE_READABLE   .callbacks = &table_fmt_machine_readable

/***
 * [[ Usage ]]
 * The table works as follows:
 * The table can be used after table_init is called. Then at the beginning of each printing, the
 * table_start function must be called. After printing, the table_end must be called. The
 * table_start MUST be paired with table_end. Inbetween table_start/table_end the user can set the
 * cells of one row and one row is finished and printed using table_end_of_row. The pairs
 * table_start/table_end can be used multiple-times for one table. The table is deallocated using
 * table_cleanup. After table_cleanup is called it is not possible to further use the struct table.
 * The struct table must be reinitialized.
 *
 * Default behaviour of the table_set_col_* is replacement of already set data. To append, the user
 * must use table_append_*
 *
 * To summarize:
 * 1) @table_init is called;
 * 2) @table_start is called following by table_set_xxx functions and @table_end.
 *    table_start/table_end forms 1-level parenthesis structure. Some of the table
 *    settings can be changed only between table_init and @table_start or after table_end
 *    is called (but before next table_start.
 * 3) the table is deallocated using @table_cleanup. After the cleanup
 *    is done, the struct table is unusable and must be initialized.
 *
 *
 * An example of the procedure is following sequence of calls:
 *  table_init
 *
 *  table_start
 *  table_end
 *  table_start
 *  table_end
 *
 *  table_cleanup
 *
 * The tableprinter supports user-specified callback for each row and table-print (i.e., a callback
 * that is called in table_end).
 *
 * The table is initialized by defining a table struct using the following macros:
 *  o TBL_START_COLUMNS indicates start of definition of columns
 *  o TBL_COL_XXX    macros specify the column types with some default formatting the column is specified using a column
 *                   name (which should be C identifier) and a prefix.  the column name is the a string with the column
 *                   name. The prefix is used for discriminating between columns from different tables. The column index
 *                   should be taken from an enum. The enum identifier is prefix concatenated with the column name identifier.
 *  o TBL_COL_XXX_F  macros specify column types with user supplied formatting
 *  o TBL_COL_END    indicates end of column definitions
 *  o TBL_COL_ORDER  specify the column order
 *  o TBL_COL_DELIMITER specify the in-between cell delimiter
 *
 * The table cells have strict type control, with the exception of type TBL_COL_ANY. In the case of
 * TBL_COL_ANY, the type is not tested and an arbitrary value can be printed into the cell.
 * It is also possible to print string to an arbitrary cell.
 *
 * Features:
 * * user supplied callback functions can be used for modifying the output format.
 *
 * Non-tested features:
 * * computing statistics of columns via the table_start_callback/table_end_callback.
 *   TODO: is it better to have callback for each cell with the original value supplied by the caller of the table_set_* functions?
 * TODO:
 * * unsupported: (dynamic) alignment of cells which is computed in table_end
 *
 * TODO: table_set_col_fmt: this functin takes the format string and the value. But I'm not able to
 * test whether the format string and the type match !!!
 *
 * TODO: Return value of the parser should be a string allocated on the mempool of the table. But:
 * is the return value really necessary? The error should be show to the user on the terminal
 * (std. out).
 * TODO: all macros prefix TBL_ should be changed to TABLE_ ?
 * TODO: how to print column which is aligned to the left flag for alignment: 1) left; 2) right;
 *       3) decimal point alignment; 4) arbitrary separator alignment
 ***/

struct table;

/** Specification of a single table column */
struct table_column {
  const char *name;      // [*] name of the column displayed in the header
  int width;             // [*] width of the column (in characters). Negative number indicates alignment to left. SHOULD BE RATHER INDICATED BY A FLAG?
  const char *fmt;       // [*] default format of each cell in the column
  enum column_type type; // type of the cells in the column.
};

struct table_output_callbacks {
  int (*row_output_func)(struct table *tbl);       // [*] function that outputs one row
  int (*table_start_callback)(struct table *tbl);  // [*] table_start callback
  int (*table_end_callback)(struct table *tbl);    // [*] table_end callback
	// FIXME: Int -> void?
  int (*process_option)(struct table *tbl, const char *key, const char *value);
	// FIXME: Shouldn't it be possible to return also a custom error string? For example in an optionally writeable `const char **' argument.
};

/** The definition of a table. Contains column definitions plus internal data. */
struct table {
  struct table_column *columns;    // [*] columns definition
  int column_count;                // [*] number of columns of the table
  struct mempool *pool;            // memory pool used for storing all the data. At the beggining are the data needed for printing
                                   // the whole table (delimited by table_start, table_end)
  struct mempool_state pool_state; // state of the pool AFTER the table is initialized. The state is used for
                                   // deallocation of the strings used for printing single table row

  char **col_str_ptrs; // used to store the position of the row in the memory pool

  uint *column_order;              // [*] order of the columns in the print-out of the table.
  uint cols_to_output;             // [*] number of columns that are printed.
  const char *col_delimiter;      // [*] delimiter that is placed between the columns
  const char *append_delimiter;   // [*] character used for delimiting the values in a single cell
  uint print_header;               // [*] 0 indicates that the header should not be printed

  struct fastbuf *out;      // fastbuffer that is used for outputing the table rows
  int last_printed_col;     // index of the last column which was set. -1 indicates start of row. Used for example for
                            // appending to the last column.
  int row_printing_started; // this can be considered as a duplicity of last_printed_col (it is -1 if the row printing
                            // did not start), but it is probably better to store the flag separately

  struct fbpool fb_col_out; // used for printing using the fast buffers(into the mempool), @see table_col_fbstart()
  int col_out;              // index of the column that is currently printed using fb_col_out

  struct table_output_callbacks *callbacks;
  void *data; // [*] user-managed data. the data can be used for example in row_output_func, table_start_callback, table_end_callback
};


/**
 * table_init serves for initialization of the table. The @tbl parameter should have set the columns member of
 * the table structure. The @out parameter is supplied by the caller and can be deallocated after table_deinit
 * is called.
 **/
void table_init(struct table *tbl, struct fastbuf *out);
void table_cleanup(struct table *tbl);

/**
 * table_start is called before the cells of the table are set. After the table_start is called, the user can
 * call the table_set_* functions. The table_end_of_row function can be called after the table_start is called
 * (but before the table_end is called)
 **/
void table_start(struct table *tbl);

/**
 * This function must be called after all the rows of the current table are printed. The table_set_*
 * functions can be called in between table_start and table_end calls.
 **/
void table_end(struct table *tbl);

/**
 * Sets the order in which the columns are printed. The @col_order parameter is used until the table_end or
 * table_cleanup is called. The table stores the pointer only and the memory pointed to by @col_order is
 * allocated and deallocated by the caller.
 **/
void table_col_order(struct table *tbl, int *col_order, int col_order_size);

/**
 * Sets the order in which the columns are printed. The specification is a string with comma delimited column
 * names.
 **/
int table_col_order_by_name(struct table *tbl, const char *col_order);

/**
 * Called when all the cells have filled values. The function the prints a table row into the output stream.
 * The table row has newline at the end.
 **/
void table_end_row(struct table *tbl);

/**
 * Prints a string that is printf-like formated into a particular column. This function does not check the
 * type of the column, i.e., it can be used to print double into an int column
 **/
void table_set_printf(struct table *tbl, int col, const char *fmt, ...) FORMAT_CHECK(printf, 3, 4);

/**
 * Appends a string that is printf-like formated to the last printed column. This function does not check the
 * type of the column, i.e., it can be used to print double into an int column
 **/
void table_append_printf(struct table *tbl, const char *fmt, ...) FORMAT_CHECK(printf, 2, 3);

/**
 * Find the index of a column with name @col_name and returns it. Returns -1 if the column was not found.
 **/
int table_get_col_idx(struct table *tbl, const char *col_name);

/**
 * Returns comma-separated list of column names
 **/
const char * table_get_col_list(struct table *tbl);

/**
 * Opens a fastbuf stream that can be used for creating a cell content. The @sz parameter is the initial size
 * allocated on the memory pool.
 **/
struct fastbuf *table_col_fbstart(struct table *tbl, int col);
// FIXME: test table_col_fbstart/table_col_fbend

/**
 * Closes the stream that is used for printing of the last column
 **/
void table_col_fbend(struct table *tbl);

/**
 * Sets the callbacks in @tbl. The callbacks are stored the arg @callbacks.
 **/
void table_set_output_callbacks(struct table *tbl, struct table_output_callbacks *callbacks);


/**
 * Process the table one option and sets the values in @tbl according to the command-line parameters.
 * The option has the following format: a) "<key>:<value>"; b) "<key>" (currently not used).
 *
 * Possible key-value pairs:
 * header:[0|1]                     - 1 indicates that the header should be printed, 0 otherwise
 * noheader                         - equivalent to header:0
 * cols:<string-with-col-names>     - comma-separated list of columns that will be printed (in the order specified on the cmd-line)
 * fmt:[human|machine|...]          - output type
 * col-delim:<char>                 - column delimiter
 *
 * Returns NULL on success or an error string otherwise.
 **/
const char *table_set_option(struct table *tbl, const char *opt);
const char *table_set_gary_options(struct table *tbl, char **gary_table_opts);

extern struct table_output_callbacks table_fmt_human_readable;
extern struct table_output_callbacks table_fmt_machine_readable;

#define TABLE_SET_COL_PROTO(_name_, _type_) void table_set_##_name_(struct table *tbl, int col, _type_ val);\
  void table_set_##_name_##_name(struct table *tbl, const char *col_name, _type_ val);\
  void table_set_##_name_##_fmt(struct table *tbl, int col, const char *fmt, _type_ val) FORMAT_CHECK(printf, 3, 0);

// table_set_<type>_fmt has one disadvantage: it is not possible to
// check whether fmt contains format that contains formatting that is
// compatible with _type_

TABLE_SET_COL_PROTO(int, int);
TABLE_SET_COL_PROTO(uint, uint);
TABLE_SET_COL_PROTO(double, double);
TABLE_SET_COL_PROTO(str, const char *);
TABLE_SET_COL_PROTO(intmax, intmax_t);
TABLE_SET_COL_PROTO(uintmax, uintmax_t);

void table_set_bool(struct table *tbl, int col, uint val);
void table_set_bool_name(struct table *tbl, const char *col_name, uint val);
void table_set_bool_fmt(struct table *tbl, int col, const char *fmt, uint val);
#undef TABLE_SET_COL_PROTO

#define TABLE_APPEND_PROTO(_name_, _type_) void table_append_##_name_(struct table *tbl, _type_ val)
TABLE_APPEND_PROTO(int, int);
TABLE_APPEND_PROTO(uint, uint);
TABLE_APPEND_PROTO(double, double);
TABLE_APPEND_PROTO(str, const char *);
TABLE_APPEND_PROTO(intmax, intmax_t);
TABLE_APPEND_PROTO(uintmax, uintmax_t);
void table_append_bool(struct table *tbl, int val);
#undef TABLE_APPEND_PROTO

#endif
