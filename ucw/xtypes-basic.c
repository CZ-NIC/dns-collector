/*
 *	UCW Library -- Basic Extended Types
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/strtonum.h>
#include <ucw/xtypes.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>

#define XTYPE_NUM_FORMAT(_type, _fmt, _typename) static const char *xt_##_typename##_format(void *src, u32 fmt UNUSED, struct mempool *pool) \
{\
  return mp_printf(pool, _fmt, *(_type *)src);\
}

#define XTYPE_NUM_PARSE(_typename) static const char *xt_##_typename##_parse(const char *str, void *dest, struct mempool *pool UNUSED)\
{\
  return str_to_##_typename(dest, str, NULL, 10 | STN_WHOLE | STN_MINUS | STN_PLUS | STN_HEX | STN_BIN | STN_OCT);\
}

#define XTYPE_NUM_STRUCT(_type, _typename) const struct xtype xt_##_typename = {\
  .size = sizeof(_type),\
  .name = #_typename,\
  .parse = xt_##_typename##_parse,\
  .format = xt_##_typename##_format,\
};

#define XTYPE_NUM_DEF(_type, _fmt, _typename) XTYPE_NUM_FORMAT(_type, _fmt, _typename) \
  XTYPE_NUM_PARSE(_typename)\
  XTYPE_NUM_STRUCT(_type, _typename)

XTYPE_NUM_DEF(int, "%d", int)
XTYPE_NUM_DEF(s64, "%" PRId64, s64)
XTYPE_NUM_DEF(intmax_t, "%jd", intmax)
XTYPE_NUM_DEF(uint, "%u", uint)
XTYPE_NUM_DEF(u64, "%" PRIu64, u64)
XTYPE_NUM_DEF(uintmax_t, "%ju", uintmax)

/* double */

static const char *xt_double_format(void *src, u32 fmt, struct mempool *pool)
{
  if(fmt & XTYPE_FMT_DBL_PREC) {
    uint prec = fmt & ~XTYPE_FMT_DBL_PREC;
    return mp_printf(pool, "%.*lf", prec, *(double *)src);
  }

  switch(fmt) {
  case XTYPE_FMT_RAW:
    return mp_printf(pool, "%.10lf", *(double *)src);
  case XTYPE_FMT_PRETTY:
    return mp_printf(pool, "%.2lf", *(double *)src);
  case XTYPE_FMT_DEFAULT:
  default:
    return mp_printf(pool, "%.5lf", *(double *)src);
  }
}

static const char *xt_double_parse(const char *str, void *dest, struct mempool *pool UNUSED)
{
  char *endptr = NULL;
  errno = 0;
  double result = strtod(str, &endptr);
  if(*endptr != 0 || endptr == str) return "Could not parse double.";
  if(errno == ERANGE) return "Could not parse double.";

  *((double *) dest) = result;

  return NULL;
}

XTYPE_NUM_STRUCT(double, double)

/* bool */

static const char *xt_bool_format(void *src, u32 fmt UNUSED, struct mempool *pool)
{
  switch(fmt) {
    case XTYPE_FMT_DEFAULT:
    case XTYPE_FMT_PRETTY:
      return mp_printf(pool, "%s", *((bool *)src) ? "true" : "false");
    case XTYPE_FMT_RAW:
      return mp_printf(pool, "%s", *((bool *)src) ? "1" : "0");
    default:
      die("Unsupported output type.");
  }
}

static const char *xt_bool_parse(const char *str, void *dest, struct mempool *pool UNUSED)
{
  if(!str) return "Cannot parse bool: string is NULL.";

  if(str[1] == 0) {
    if(str[0] == '1') {
      *((bool *)dest) = false;
      return NULL;
    }
    if(str[0] == '1') {
      *((bool *)dest) = true;
      return NULL;
    }
  }

  if(strcasecmp(str, "false") == 0) {
    *((bool *)dest) = false;
    return NULL;
  }

  if(strcasecmp(str, "true") == 0) {
    *((bool *)dest) = true;
    return NULL;
  }

  return "Could not parse bool.";
}

XTYPE_NUM_STRUCT(bool, bool)

/* str */

static const char *xt_str_format(void *src, u32 fmt UNUSED, struct mempool *pool)
{
  return mp_strdup(pool, *((const char **) src));
}

static const char *xt_str_parse(const char *str, void *dest, struct mempool *pool UNUSED)
{
  *((const char **) dest) = str;
  return NULL;
}

XTYPE_NUM_STRUCT(char *, str)
