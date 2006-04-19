/*
 *	UCW Library -- Reading of configuration files
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf2.h"
#include "lib/mempool.h"

#include <stdlib.h>
#include <errno.h>

/* Memory allocation */

static struct mempool *cf_pool;

void *
cf_malloc(uns size)
{
  return mp_alloc(cf_pool, size);
}

void *
cf_malloc_zero(uns size)
{
  return mp_alloc_zero(cf_pool, size);
}

byte *
cf_strdup(byte *s)
{
  return mp_strdup(cf_pool, s);
}

byte *
cf_printf(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  byte *res = mp_vprintf(cf_pool, fmt, args);
  va_end(args);
  return res;
}

/* Undo journal */

static uns journal_active;

uns
cf_journal_active(uns flag)
{
  uns f = journal_active;
  journal_active = flag;
  return f;
}

void
cf_journal_block(void *ptr UNUSED, uns len UNUSED)
{
}

/* Parsers for standard types */

struct unit {
  uns name;			// one-letter name of the unit
  uns num, den;			// fraction
};

static const struct unit units[] = {
  { 'd', 86400, 1 },
  { 'h', 3600, 1 },
  { 'k', 1000, 1 },
  { 'm', 1000000, 1 },
  { 'g', 1000000000, 1 },
  { 'K', 1024, 1 },
  { 'M', 1048576, 1 },
  { 'G', 1073741824, 1 },
  { '%', 1, 100 },
  { 0, 0, 0 }
};

static const struct unit *
lookup_unit(byte *value, byte *end, byte **msg)
{
  if (end && *end) {
    if (end == value || end[1] || *end >= '0' && *end <= '9')
      *msg = "Invalid number";
    else {
      for (const struct unit *u=units; u->name; u++)
	if (u->name == *end)
	  return u;
      *msg = "Invalid unit";
    }
  }
  return NULL;
}

static char cf_rngerr[] = "Number out of range";

static byte *
cf_parse_int(uns number, byte **pars, int *ptr)
{
  cf_journal_block(ptr, number * sizeof(int));
  for (uns i=0; i<number; i++)
  {
    byte *msg = NULL;
    if (!*pars[i])
      msg = "Missing number";
    else {
      const struct unit *u;
      char *end;
      errno = 0;
      uns x = strtoul(pars[i], &end, 0);
      if (errno == ERANGE)
	msg = cf_rngerr;
      else if (u = lookup_unit(pars[i], end, &msg)) {
	u64 y = (u64)x * u->num;
	if (y % u->den)
	  msg = "Number is not an integer";
	else {
	  y /= u->den;
	  if (y > 0xffffffff)
	    msg = cf_rngerr;
	  ptr[i] = y;
	}
      } else
	ptr[i] = x;
    }
    if (msg)
      return number==1 ? msg : cf_printf("Item #%d: %s", i+1, msg);
  }
  return NULL;
}

static byte *
cf_parse_u64(uns number, byte **pars, u64 *ptr)
{
  cf_journal_block(ptr, number * sizeof(u64));
  for (uns i=0; i<number; i++)
  {
    byte *msg = NULL;
    if (!*pars[i])
      msg = "Missing number";
    else {
      const struct unit *u;
      char *end;
      errno = 0;
      u64 x = strtoull(pars[i], &end, 0);
      if (errno == ERANGE)
	msg = cf_rngerr;
      else if (u = lookup_unit(pars[i], end, &msg)) {
	if (x > ~(u64)0 / u->num)
	  msg = "Number out of range";
	else {
	  x *= u->num;
	  if (x % u->den)
	    msg = "Number is not an integer";
	  else
	    ptr[i] = x / u->den;
	}
      } else
	ptr[i] = x;
    }
    if (msg)
      return number==1 ? msg : cf_printf("Item #%d: %s", i+1, msg);
  }
  return NULL;
}

static byte *
cf_parse_double(uns number, byte **pars, double *ptr)
{
  cf_journal_block(ptr, number * sizeof(double));
  for (uns i=0; i<number; i++)
  {
    byte *msg = NULL;
    if (!*pars[i])
      msg = "Missing number";
    else {
      const struct unit *u;
      char *end;
      errno = 0;
      double x = strtoul(pars[i], &end, 0);
      if (errno == ERANGE)
	msg = cf_rngerr;
      else if (u = lookup_unit(pars[i], end, &msg))
	ptr[i] = x * u->num / u->den;
      else
	ptr[i] = x;
    }
    if (msg)
      return number==1 ? msg : cf_printf("Item #%d: %s", i+1, msg);
  }
  return NULL;
}

static byte *
cf_parse_string(uns number, byte **pars, byte **ptr)
{
  cf_journal_block(ptr, number * sizeof(byte*));
  for (uns i=0; i<number; i++)
    ptr[i] = cf_strdup(pars[i]);
  return NULL;
}

static byte *
cf_parse_int_ary(uns number, byte **pars, int **ptr)
{
  cf_journal_block(ptr, sizeof(int**));
  *ptr = (int*) cf_malloc((number+1) * sizeof(int*)) + 1;
  ARRAY_LEN(*ptr) = number;
  uns old_flag = cf_journal_active(0);
  byte *msg = cf_parse_int(number, pars, *ptr);
  cf_journal_active(old_flag);
  return msg;
}

static byte *
cf_parse_u64_ary(uns number, byte **pars, u64 **ptr)
{
  cf_journal_block(ptr, sizeof(u64**));
  *ptr = (u64*) cf_malloc((number+1) * sizeof(u64*)) + 1;
  ARRAY_LEN(*ptr) = number;
  uns old_flag = cf_journal_active(0);
  byte *msg = cf_parse_u64(number, pars, *ptr);
  cf_journal_active(old_flag);
  return msg;
}

static byte *
cf_parse_double_ary(uns number, byte **pars, double **ptr)
{
  cf_journal_block(ptr, sizeof(double**));
  *ptr = (double*) cf_malloc((number+1) * sizeof(double*)) + 1;
  ARRAY_LEN(*ptr) = number;
  uns old_flag = cf_journal_active(0);
  byte *msg = cf_parse_double(number, pars, *ptr);
  cf_journal_active(old_flag);
  return msg;
}

static byte *
cf_parse_string_ary(uns number, byte **pars, byte ***ptr)
{
  cf_journal_block(ptr, sizeof(byte***));
  *ptr = (byte**) cf_malloc((number+1) * sizeof(byte**)) + 1;
  ARRAY_LEN(*ptr) = number;
  uns old_flag = cf_journal_active(0);
  byte *msg = cf_parse_string(number, pars, *ptr);
  cf_journal_active(old_flag);
  return msg;
}
