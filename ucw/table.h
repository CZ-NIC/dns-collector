/*
 *	UCW Library -- Table printer
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 */

#ifndef _UCW_TABLE_H
#define _UCW_TABLE_H

#include <ucw/fastbuf.h>
#include <ucw/mempool.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define table_append_bool ucw_table_append_bool
#define table_append_double ucw_table_append_double
#define table_append_int ucw_table_append_int
#define table_append_intmax ucw_table_append_intmax
#define table_append_printf ucw_table_append_printf
#define table_append_str ucw_table_append_str
#define table_append_u64 ucw_table_append_u64
#define table_append_uint ucw_table_append_uint
#define table_append_uintmax ucw_table_append_uintmax
#define table_cleanup ucw_table_cleanup
#define table_col_bool ucw_table_col_bool
#define table_col_bool_fmt ucw_table_col_bool_fmt
#define table_col_bool_name ucw_table_col_bool_name
#define table_col_double ucw_table_col_double
#define table_col_fbend ucw_table_col_fbend
#define table_col_fbstart ucw_table_col_fbstart
#define table_col_int ucw_table_col_int
#define table_col_intmax ucw_table_col_intmax
#define table_col_printf ucw_table_col_printf
#define table_col_str ucw_table_col_str
#define table_col_u64 ucw_table_col_u64
#define table_col_s64 ucw_table_col_s64
#define table_col_uint ucw_table_col_uint
#define table_col_uintmax ucw_table_col_uintmax
#define table_end ucw_table_end
#define table_end_row ucw_table_end_row
#define table_fmt_human_readable ucw_table_fmt_human_readable
#define table_fmt_machine_readable ucw_table_fmt_machine_readable
#define table_get_col_idx ucw_table_get_col_idx
#define table_get_col_list ucw_table_get_col_list
#define table_init ucw_table_init
#define table_set_col_order ucw_table_set_col_order
#define table_set_col_order_by_name ucw_table_set_col_order_by_name
#define table_set_formatter ucw_table_set_formatter
#define table_set_gary_options ucw_table_set_gary_options
#define table_set_option ucw_table_set_option
#define table_set_option_value ucw_table_set_option_value
#define table_start ucw_table_start
#endif

enum column_type {
  COL_TYPE_STR,
  COL_TYPE_INT,
  COL_TYPE_S64,
  COL_TYPE_INTMAX,
  COL_TYPE_UINT,
  COL_TYPE_U64,
  COL_TYPE_UINTMAX,
  COL_TYPE_BOOL,
  COL_TYPE_DOUBLE,
  COL_TYPE_ANY,
  COL_TYPE_LAST
};

#define CELL_ALIGN_LEFT     (1<<(sizeof(enum column_type)*8 - 1))
// FIXME: an example of another flag, not implemented now
#define CELL_ALIGN_CENTER   (1<<(sizeof(enum column_type)*8 - 2))
#define CELL_ALIGN_FLOAT    (1<<(sizeof(enum column_type)*8 - 3))

#define CELL_TRUNC_RIGHT    (1<<(sizeof(enum column_type)*8 - 4))

// CELL_ALIGN_MASK is a mask which has 1's on positions used by some alignment mask.
// that is: col_width & CELL_ALIGN_MASK  gives column width (in characters).
// the top bit is reserved for left alignment and is not demasked by CELL_ALIGN_MASK.
// the reason is that all printf and friends are using negative number for left alignment.
#define CELL_ALIGN_MASK     (~(CELL_ALIGN_LEFT | CELL_ALIGN_FLOAT | CELL_ALIGN_CENTER))

#define TBL_COL_STR(_name, _width)            { .name = _name, .width = _width, .fmt = "%s", .type = COL_TYPE_STR }
#define TBL_COL_INT(_name, _width)            { .name = _name, .width = _width, .fmt = "%d", .type = COL_TYPE_INT }
#define TBL_COL_S64(_name, _width)            { .name = _name, .width = _width, .fmt = "%lld", .type = COL_TYPE_S64 }
#define TBL_COL_UINT(_name, _width)           { .name = _name, .width = _width, .fmt = "%u", .type = COL_TYPE_UINT }
#define TBL_COL_U64(_name, _width)            { .name = _name, .width = _width, .fmt = "%llu", .type = COL_TYPE_U64 }
#define TBL_COL_INTMAX(_name, _width)         { .name = _name, .width = _width, .fmt = "%jd", .type = COL_TYPE_INTMAX }
#define TBL_COL_UINTMAX(_name, _width)        { .name = _name, .width = _width, .fmt = "%ju", .type = COL_TYPE_UINTMAX }
#define TBL_COL_HEXUINT(_name, _width)        { .name = _name, .width = _width, .fmt = "0x%x", .type = COL_TYPE_UINT }
#define TBL_COL_DOUBLE(_name, _width, _prec)  { .name = _name, .width = _width, .fmt = "%." #_prec "lf", .type = COL_TYPE_DOUBLE }
#define TBL_COL_BOOL(_name, _width)           { .name = _name, .width = _width, .fmt = "%s", .type = COL_TYPE_BOOL }
#define TBL_COL_ANY(_name, _width)            { .name = _name, .width = _width, .fmt = 0, .type = COL_TYPE_ANY }

#define TBL_COL_STR_FMT(_name, _width, _fmt)            { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_STR }
#define TBL_COL_INT_FMT(_name, _width, _fmt)            { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_INT }
#define TBL_COL_S64_FMT(_name, _width, _fmt)            { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_S64 }
#define TBL_COL_UINT_FMT(_name, _width, _fmt)           { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_UINT }
#define TBL_COL_U64_FMT(_name, _width, _fmt)            { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_U64 }
#define TBL_COL_INTMAX_FMT(_name, _width, _fmt)         { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_INTMAX }
#define TBL_COL_UINTMAX_FMT(_name, _width, _fmt)        { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_UINTMAX }
#define TBL_COL_HEXUINT_FMT(_name, _width, _fmt)        { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_UINT }
#define TBL_COL_BOOL_FMT(_name, _width, _fmt)           { .name = _name, .width = _width, .fmt = _fmt, .type = COL_TYPE_BOOL }

#define TBL_COL_END { .name = 0, .width = 0, .fmt = 0, .type = COL_TYPE_LAST }

#define TBL_COLUMNS  .columns = (struct table_column [])
#define TBL_COL_ORDER(order) .column_order = (int *) order, .cols_to_output = ARRAY_SIZE(order)
#define TBL_COL_DELIMITER(_delimiter_) .col_delimiter = _delimiter_
#define TBL_APPEND_DELIMITER(_delimiter_) .append_delimiter = _delimiter_

#define TBL_OUTPUT_HUMAN_READABLE     .formatter = &table_fmt_human_readable
#define TBL_OUTPUT_BLOCKLINE          .formatter = &table_fmt_blockline
#define TBL_OUTPUT_MACHINE_READABLE   .formatter = &table_fmt_machine_readable

/***
 * [[ Usage ]]
 * The table works as follows:
 * The table can be used after table_init is called. Then at the beginning of each printing, the
 * table_start function must be called. After printing, the table_end must be called. The
 * table_start MUST be paired with table_end. Inbetween table_start/table_end the user can set the
 * cells of one row and one row is finished and printed using table_end_row. The pairs
 * table_start/table_end can be used multiple-times for one table. The table is deallocated using
 * table_cleanup. After table_cleanup is called it is not possible to further use the struct table.
 * The struct table must be reinitialized.
 *
 * Default behaviour of the table_col_* is replacement of already set data. To append, the user
 * must use table_append_*
 *
 * To summarize:
 * 1) @table_init is called;
 * 2) @table_start is called following by table_col_xxx functions and @table_end.
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
 *  o TBL_COLUMNS    indicates start of definition of columns
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
 * - user supplied callback functions can be used for modifying the output format.
 *
 * Command-line options (provided through table_set_option and friends):
 *   key      value                       explanation
 *  header    [0|1]                       0=do not print the header
 *  cols      <col-name>[,<col-name>]*    list of column names delimited by comma ','
 *            '*'                         or star '*' meaning all columns
 *   fmt      [human|machine|blockline]   output format
 * col-delim  <string>                    string used as a column delimiter
 *
 ***/

struct table;

/** Specification of a single table column */
struct table_column {
  const char *name;		// [*] Name of the column displayed in table header
  int width;			// [*] Width of the column (in characters). Negative number indicates alignment to left.
				// FIXME: Request left alignment by a flag.
  const char *fmt;		// [*] Default format of each cell in the column
  enum column_type type;	// Type of the cells in the column
};

/** The definition of a table. Contains column definitions plus internal data. */
struct table {
  struct table_column *columns;		// [*] Definition of columns
  int column_count;			// [*] Number of columns (calculated by table_init())
  struct mempool *pool;			// Memory pool used for storing table data. Contains global state
					// and data of the current row.
  struct mempool_state pool_state;	// State of the pool after the table is initialized, i.e., before
					// per-row data have been allocated.

  char **col_str_ptrs;			// Values of cells in the current row (allocated from the pool)

  uint *column_order;			// [*] Order of the columns in the print-out of the table
  uint cols_to_output;			// [*] Number of columns that are printed
  const char *col_delimiter;		// [*] Delimiter that is placed between columns
  const char *append_delimiter;		// [*] Separator of multiple values in a single cell (see table_append_...())
  uint print_header;			// [*] 0 indicates that table header should not be printed

  struct fastbuf *out;			// [*] Fastbuffer to which the table is printed
  int last_printed_col;			// Index of the last column which was set. -1 indicates start of row.
					// Used for example for appending to the current column.
  int row_printing_started;		// Indicates that a row has been started. Duplicates last_printed_col, but harmlessly.
  struct fbpool fb_col_out;		// Per-cell fastbuf, see table_col_fbstart()
  int col_out;				// Index of the column that is currently printed using fb_col_out

  // Back-end used for table formatting and its private data
  struct table_formatter *formatter;
  void *data;
};


/**
 * @table_init serves for initialization of the table. The @tbl parameter should have set the columns member of
 * the table structure.
 **/
void table_init(struct table *tbl);
void table_cleanup(struct table *tbl);

/**
 * table_start is called before the cells of the table are set. After the table_start is called, the
 * user can call the table_col_* or table_append_ functions, but cannot call the table_set_*
 * functions. The table_end_row function can be called after the table_start is called (but before
 * the table_end is called)
 **/
void table_start(struct table *tbl, struct fastbuf *out);

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
void table_set_col_order(struct table *tbl, int *col_order, int col_order_size);

/**
 * Sets the order in which the columns are printed. The specification is a string with comma-separated column
 * names. Returns NULL for success and an error message otherwise.
 **/
const char *table_set_col_order_by_name(struct table *tbl, const char *col_order);

/**
 * Called when all the cells have filled values. The function the prints a table row into the output stream.
 * The table row has newline at the end.
 **/
void table_end_row(struct table *tbl);

/**
 * Cleans current row, When called.
 **/
void table_clean_row(struct table *tbl);

/**
 * Prints a string that is printf-like formated into a particular column. This function does not check the
 * type of the column, i.e., it can be used to print double into an int column
 **/
void table_col_printf(struct table *tbl, int col, const char *fmt, ...) FORMAT_CHECK(printf, 3, 4);

/**
 * Appends a string that is printf-like formated to the last printed column. This function does not check the
 * type of the column, i.e., it can be used to print double into an int column.
 **/
void table_append_printf(struct table *tbl, const char *fmt, ...) FORMAT_CHECK(printf, 2, 3);

/**
 * Find the index of a column with name @col_name and returns it. Returns -1 if the column was not found.
 **/
int table_get_col_idx(struct table *tbl, const char *col_name);

/**
 * Returns comma-and-space-separated list of column names, allocated from table's internal
 * memory pool.
 **/
const char *table_get_col_list(struct table *tbl);

/**
 * Opens a fastbuf stream that can be used for creating a cell content. The @sz parameter is the initial size
 * allocated on the memory pool.
 **/
struct fastbuf *table_col_fbstart(struct table *tbl, int col);

/**
 * Closes the stream that is used for printing of the last column.
 **/
void table_col_fbend(struct table *tbl);

/**
 * Sets table formatter for @tbl.
 **/
void table_set_formatter(struct table *tbl, struct table_formatter *fmt);

/** Definition of a formatter back-end. **/
struct table_formatter {
  void (*row_output)(struct table *tbl);	// [*] Function that outputs one row
  void (*table_start)(struct table *tbl);	// [*] table_start callback (optional)
  void (*table_end)(struct table *tbl);		// [*] table_end callback (optional)
  bool (*process_option)(struct table *tbl, const char *key, const char *value, const char **err);
	// [*] Process table option and possibly return an error message (optional)
};

// Standard formatters
extern struct table_formatter table_fmt_human_readable;
extern struct table_formatter table_fmt_machine_readable;
extern struct table_formatter table_fmt_blockline;

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
const char *table_set_option_value(struct table *tbl, const char *key, const char *value);
const char *table_set_gary_options(struct table *tbl, char **gary_table_opts);

#define TABLE_COL_PROTO(_name_, _type_) void table_col_##_name_(struct table *tbl, int col, _type_ val);\
  void table_col_##_name_##_name(struct table *tbl, const char *col_name, _type_ val);\
  void table_col_##_name_##_fmt(struct table *tbl, int col, const char *fmt, _type_ val) FORMAT_CHECK(printf, 3, 0);

// table_col_<type>_fmt has one disadvantage: it is not possible to
// check whether fmt contains format that contains formatting that is
// compatible with _type_

TABLE_COL_PROTO(int, int);
TABLE_COL_PROTO(uint, uint);
TABLE_COL_PROTO(double, double);
TABLE_COL_PROTO(str, const char *);
TABLE_COL_PROTO(intmax, intmax_t);
TABLE_COL_PROTO(uintmax, uintmax_t);
TABLE_COL_PROTO(s64, s64);
TABLE_COL_PROTO(u64, u64);

void table_col_bool(struct table *tbl, int col, uint val);
void table_col_bool_name(struct table *tbl, const char *col_name, uint val);
void table_col_bool_fmt(struct table *tbl, int col, const char *fmt, uint val);
#undef TABLE_COL_PROTO

#define TABLE_APPEND_PROTO(_name_, _type_) void table_append_##_name_(struct table *tbl, _type_ val)
TABLE_APPEND_PROTO(int, int);
TABLE_APPEND_PROTO(uint, uint);
TABLE_APPEND_PROTO(double, double);
TABLE_APPEND_PROTO(str, const char *);
TABLE_APPEND_PROTO(intmax, intmax_t);
TABLE_APPEND_PROTO(uintmax, uintmax_t);
TABLE_APPEND_PROTO(s64, s64);
TABLE_APPEND_PROTO(u64, u64);
void table_append_bool(struct table *tbl, int val);
#undef TABLE_APPEND_PROTO

#endif
