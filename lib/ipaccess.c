/*
 *	UCW Library -- IP address access lists
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/lists.h"
#include "lib/conf.h"
#include "lib/chartype.h"
#include "lib/ipaccess.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

struct ipaccess_list {
  list l;
};

struct ipaccess_entry {
  node n;
  uns allow;
  u32 addr, mask;
};

struct ipaccess_list *
ipaccess_init(void)
{
  /* Cannot use cfg_malloc() here as the pool can be uninitialized now */
  struct ipaccess_list *l = xmalloc(sizeof(*l));

  init_list(&l->l);
  return l;
}

static byte *
parse_ip(byte **p, u32 *varp)
{
  while (Cspace(**p))
    (*p)++;
  if (!**p)
    return "Missing IP address";
  uns x = 0;
  if (**p == '0' && *(*p + 1) | 32 == 'X')
    {
      errno = 0;
      x = strtoul(*p + 2, (char **)p, 16);
      if (errno == ERANGE || x > 0xffffffff)
        goto error;
    }
  else
    for (uns i = 0; i < 4; i++)
      {
        if (i)
          {
            while (Cspace(**p))
              (*p)++;
            if (*(*p)++ != '.')
              goto error;
          }
        while (Cspace(**p))
          (*p)++;
        errno = 0;
        uns y = strtoul(*p, (char **)p, 10);
        if (errno == ERANGE || y > 255)
          goto error;
        x = (x << 8) + y;
      }
  *varp = x;
  return NULL;
error:
  return "Invalid IP address";
}

byte *
ipaccess_parse(struct ipaccess_list *l, byte *c, int is_allow)
{
  byte *p = strchr(c, '/');
  char *q;
  struct ipaccess_entry *a = cfg_malloc(sizeof(struct ipaccess_entry));
  unsigned long pxlen;

  a->allow = is_allow;
  a->mask = ~0U;
  if (p)
    {
      *p++ = 0;
      pxlen = strtoul(p, &q, 10);
      if ((!q || !*q) && pxlen <= 32)
	{
	  if (pxlen != 32)
	    a->mask = ~(~0U >> (uns) pxlen);
	}
      else if (q = parse_ip(&p, &a->mask))
	return q;
    }
  add_tail(&l->l, &a->n);
  return parse_ip(&c, &a->addr);
}

int
ipaccess_check(struct ipaccess_list *l, u32 ip)
{
  struct ipaccess_entry *a;

  DO_FOR_ALL(a, l->l)
    if (! ((ip ^ a->addr) & a->mask))
      return a->allow;
  return 0;
}
