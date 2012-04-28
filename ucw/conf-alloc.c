/*
 *	UCW Library -- Configuration files: memory allocation
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/conf-internal.h>
#include <ucw/mempool.h>

inline struct mempool *
cf_get_pool(void)
{
  return cf_get_context()->pool;
}

void *
cf_malloc(uns size)
{
  return mp_alloc(cf_get_pool(), size);
}

void *
cf_malloc_zero(uns size)
{
  return mp_alloc_zero(cf_get_pool(), size);
}

char *
cf_strdup(const char *s)
{
  return mp_strdup(cf_get_pool(), s);
}

char *
cf_printf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char *res = mp_vprintf(cf_get_pool(), fmt, args);
  va_end(args);
  return res;
}
