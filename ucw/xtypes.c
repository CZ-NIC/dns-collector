/*
 *	UCW Library -- Extended Types -- Generic Operations
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/xtypes.h>

#include <string.h>

static const char * const fmt_names[] = {
  [XTYPE_FMT_DEFAULT] = "default",
  [XTYPE_FMT_RAW] = "raw",
  [XTYPE_FMT_PRETTY] = "pretty",
};

const char *xtype_parse_fmt(struct xtype *xt, const char *str, u32 *dest, struct mempool *pool)
{
  for (uint i=0; i < ARRAY_SIZE(fmt_names); i++)
    if (!strcmp(str, fmt_names[i]))
      {
	*dest = i;
	return NULL;
      }

  if (xt && xt->parse_fmt)
    return (xt->parse_fmt)(str, dest, pool);
  else
    return "Unknown mode";
}

const char *xtype_format_fmt(struct xtype *xt, u32 fmt, struct mempool *pool)
{
  if (fmt & XTYPE_FMT_CUSTOM)
    {
      if (xt->format_fmt)
	return (xt->format_fmt)(fmt, pool);
    }
  else if (fmt < ARRAY_SIZE(fmt_names))
    return fmt_names[fmt];

  return "";
}

int xtype_unit_parser(const char *str, const struct unit_definition *units)
{
  for (int i=0; units[i].unit; i++)
    if (!strcmp(str, units[i].unit))
      return i;
  return -1;
}
