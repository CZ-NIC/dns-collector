/*
 *	UCW Library -- Basic Extended Types
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/strtonum.h>
#include <ucw/xtypes.h>

static const char *xt_int_parse(const char *str, void *dest, struct mempool *pool UNUSED)
{
  return str_to_int(dest, str, NULL, 10 | STN_WHOLE | STN_MINUS | STN_PLUS | STN_HEX | STN_BIN | STN_OCT);
}

static const char *xt_int_format(void *src, u32 fmt UNUSED, struct mempool *pool)
{
  return mp_printf(pool, "%d", *(int *)src);
}

const struct xtype xt_int = {
  .size = sizeof(int),
  .name = "int",
  .parse = xt_int_parse,
  .format = xt_int_format,
};
