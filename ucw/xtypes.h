/*
 *	UCW Library -- Extended Types
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_XTYPE_H
#define _UCW_XTYPE_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define xt_bool ucw_xt_bool
#define xt_double ucw_xt_double
#define xt_int ucw_xt_int
#define xt_intmax ucw_xt_intmax
#define xt_s64 ucw_xt_s64
#define xt_str ucw_xt_str
#define xt_u64 ucw_xt_u64
#define xt_uint ucw_xt_uint
#define xt_uintmax ucw_xt_uintmax
#define xtype_format_fmt ucw_xtype_format_fmt
#define xtype_parse_fmt ucw_xtype_parse_fmt
#define xtype_unit_parser ucw_xtype_unit_parser
#endif

struct mempool;

/**
 * A parsing callback. Takes a string, interprets it as a value of the particular
 * xtype and stores it where @dest points. Returns NULL on success and an error message
 * otherwise. It may allocate memory from the @pool and the parsed value can contain
 * pointers to this memory.
 **/
typedef const char * (*xtype_parser)(const char *str, void *dest, struct mempool *pool);

/**
 * A formatting callback. Takes a value of the particular xtype and a formatting
 * mode @fmt (see below for how the modes work) and returns a string representation
 * of the value. The string can be allocated from the @pool, but it does not have to.
 *
 * When @fmt is set to `XTYPE_FMT_DEFAULT`, the resulting string should be
 * parseable via the parsing callback and yield a semantically equivalent value.
 **/
typedef const char * (*xtype_formatter)(void *src, u32 fmt, struct mempool *pool);

/**
 * Formatting of values is controlled by a mode parameter, which is generally
 * a 32-bit integer. If the most significant bit is clear, it is one of generic
 * well-known modes (`XTYPE_FMT_`'something'), which can be passed to all formatters
 * and if it is not understood, it acts like `XTYPE_FMT_DEFAULT`. When the most
 * significant bit is set, the meaning of the mode is specific to the particular
 * xtype.
 **/

enum xtype_fmt {
  XTYPE_FMT_DEFAULT = 0,	// Default format: readable, but not hostile to machine parsing
  XTYPE_FMT_RAW = 1,		// Raw data with no frills
  XTYPE_FMT_PRETTY = 2,		// Try to please humans (e.g., like "ls -h")
  XTYPE_FMT_CUSTOM = 0x80000000,
};

/**
 * A callback for parsing non-generic formatting modes. See `xtype_parser` for more
 * details. It is usually called via `xtype_parse_fmt`, which handles the generic modes.
 **/
typedef const char * (*xtype_fmt_parser)(const char *str, u32 *dest, struct mempool *pool);

/**
 * A callback for constructing a string representation of non-generic formatting modes,
 * analogous to `xtype_formatter`. It is usually called via `xtype_format_fmt`,
 * which handles the generic modes. Returns an empty string for unknown modes.
 **/
typedef const char * (*xtype_fmt_formatter)(u32 fmt, struct mempool *pool);

/**
 * This structure describes an xtype. Among other things, it points to callback
 * functions handling this xtype.
 **/
struct xtype {
  size_t size;				// How many bytes does a single value occupy
  const char *name;			// Name used in debug messages
  xtype_parser parse;			// Parsing callback
  xtype_formatter format;		// Formatting callback
  xtype_fmt_parser parse_fmt;		// Format mode parsing callback (optional)
  xtype_fmt_formatter format_fmt;	// Format mode formatting callback (optional)
};

/**
 * Construct a formatting mode from its string representation. It is a wrapper
 * around the `xtype_fmt_parser` hook, which handles generic modes first.
 *
 * The generic modes are called `default`, `raw`, and `pretty`.
 **/
const char *xtype_parse_fmt(struct xtype *xt, const char *str, u32 *dest, struct mempool *pool);

/**
 * Construct a string representation of a formatting mode. It is a wrapper
 * around the `xtype_fmt_formatter` hook, which handles generic modes first.
 * Returns an empty string for unknown modes.
 **/
const char *xtype_format_fmt(struct xtype *xt, u32 fmt, struct mempool *pool);

// Basic set of extended types
extern const struct xtype xt_str;
extern const struct xtype xt_int;
extern const struct xtype xt_s64;
extern const struct xtype xt_intmax;
extern const struct xtype xt_uint;
extern const struct xtype xt_u64;
extern const struct xtype xt_uintmax;
extern const struct xtype xt_bool;
extern const struct xtype xt_double;

// Fixed-precision formats for xt_double
#define XTYPE_FMT_DBL_FIXED_PREC(_prec) (_prec | XTYPE_FMT_DBL_PREC)
#define XTYPE_FMT_DBL_PREC XTYPE_FMT_CUSTOM

/* Tables of units, provided as convenience for the implementations of xtypes */

/**
 * Definition of the units that are appended to an xtype value. The value with units is represented
 * by the following string: "<value><units>". The final value of the of the string is computed using
 * the following formula: <value> * <num>/<denom>.
 **/
struct unit_definition {
  const char *unit; // string representation of unit
  u64 num;
  u64 denom;
};

/**
 * Parse a name of a unit and return its index in the @units array or -1
 * if is not present there.
 **/
int xtype_unit_parser(const char *str, struct unit_definition *units);

#endif
