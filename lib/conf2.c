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
#include <string.h>
#include <errno.h>

/* Memory allocation */

static struct old_pools {
  struct old_pools *prev;
  struct mempool *pool;
} *pools;

void *
cf_malloc(uns size)
{
  return mp_alloc(pools->pool, size);
}

void *
cf_malloc_zero(uns size)
{
  return mp_alloc_zero(pools->pool, size);
}

byte *
cf_strdup(byte *s)
{
  return mp_strdup(pools->pool, s);
}

byte *
cf_printf(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  byte *res = mp_vprintf(pools->pool, fmt, args);
  va_end(args);
  return res;
}

/* Undo journal */

static struct journal_item {
  struct journal_item *prev;
  void *ptr;
  uns len;
  byte copy[0];
} *journal;

void
cf_journal_block(void *ptr, uns len)
{
  struct journal_item *ji = cf_malloc(sizeof(struct journal_item) + len);
  ji->prev = journal;
  ji->ptr = ptr;
  ji->len = len;
  memcpy(ji->copy, ptr, len);
  journal = ji;
}

static void
journal_swap(void)
  // swaps the contents of the memory and the journal, and reverses the list
{
  struct journal_item *curr, *prev, *next;
  uns max_len = 0;
  for (curr=journal; curr; curr=curr->prev)
    max_len = MAX(max_len, curr->len);
  byte *buf = xmalloc(max_len);
  for (next=NULL, curr=journal; curr; next=curr, curr=prev)
  {
    prev = curr->prev;
    curr->prev = next;
    memcpy(buf, curr->copy, curr->len);
    memcpy(curr->copy, curr->ptr, curr->len);
    memcpy(curr->ptr, buf, curr->len);
  }
  journal = next;
  xfree(buf);
}

static struct journal_item *
journal_new_section(uns new_pool)
{
  if (new_pool)
  {
    struct old_pools *oldp = pools;
    struct mempool *mp = mp_new(1<<14);
    pools = mp_alloc(mp, sizeof(struct old_pools));
    pools->prev = oldp;
    pools->pool = mp;
  }
  struct journal_item *oldj = journal;
  journal = NULL;
  return oldj;
}

static void
journal_commit_section(struct journal_item *oldj)
{
  struct journal_item **j = &journal;
  while (*j)
    j = &(*j)->prev;
  *j = oldj;
}

static void
journal_rollback_section(uns new_pool, struct journal_item *oldj)
{
  journal_swap();
  journal = oldj;
  if (new_pool)
  {
    struct old_pools *oldp = pools;
    pools = pools->prev;
    mp_delete(oldp->pool);
  }
}

/* Safe loading and reloading */

static byte *load_file(byte *file);
static byte *load_string(byte *string);

byte *
cf_reload(byte *file)
{
  journal_swap();
  struct old_pools *oldp = pools;
  pools = NULL;

  struct journal_item *oldj = journal_new_section(1);
  byte *msg = load_file(file);
  if (msg)
  {
    journal_rollback_section(1, oldj);
    pools = oldp;
    journal_swap();
  }
  else
    for (struct old_pools *p=oldp; p; p=oldp)
    {
      oldp = p->prev;
      mp_delete(p->pool);
    }
  return msg;
}

byte *
cf_load(byte *file)
{
  struct journal_item *oldj = journal_new_section(1);
  byte *msg = load_file(file);
  if (!msg)
    journal_commit_section(oldj);
  else
    journal_rollback_section(1, oldj);
  return msg;
}

byte *
cf_set(byte *string)
{
  struct journal_item *oldj = journal_new_section(0);
  byte *msg = load_string(string);
  if (!msg)
    journal_commit_section(oldj);
  else
    journal_rollback_section(0, oldj);
  return msg;
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

byte *
cf_parse_int(uns number, byte **pars, int *ptr)
{
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

byte *
cf_parse_u64(uns number, byte **pars, u64 *ptr)
{
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

byte *
cf_parse_double(uns number, byte **pars, double *ptr)
{
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
  for (uns i=0; i<number; i++)
    ptr[i] = cf_strdup(pars[i]);
  return NULL;
}

/* Register size of and parser for each basic type */
static struct {
  uns size;
  void *parser;
} parsers[] = {
  { sizeof(int), cf_parse_int },
  { sizeof(u64), cf_parse_u64 },
  { sizeof(double), cf_parse_double },
  { sizeof(byte*), cf_parse_string }
};

static byte *
cf_parse_dyn(uns number, byte **pars, void **ptr, enum cf_type type)
{
  cf_journal_block(ptr, sizeof(void*));
  *ptr = cf_malloc((number+1) * parsers[type].size) + parsers[type].size;
  * (uns*) (*ptr - parsers[type].size) = number;
  return ((cf_parser*) parsers[type].parser) (number, pars, *ptr);
}
