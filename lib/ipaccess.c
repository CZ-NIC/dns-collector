/*
 *	Sherlock Library -- IP address access lists
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/lists.h"
#include "lib/conf.h"
#include "lib/chartype.h"
#include "lib/ipaccess.h"

#include <string.h>

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
parse_ip(byte *x, u32 *a)
{
  uns i, q;
  u32 z = 0;

  for(i=0; i<4; i++)
    {
      q = 0;
      while (Cdigit(*x))
	{
	  q = q*10 + *x++ - '0';
	  if (q > 255)
	    return "Invalid IP address";
	}
      if (*x++ != ((i == 3) ? 0 : '.'))
	return "Invalid IP address";
      z = (z << 8) | q;
    }
  *a = z;
  return NULL;
}

byte *
ipaccess_parse(struct ipaccess_list *l, byte *c, int is_allow)
{
  byte *p = strchr(c, '/');
  byte *q;
  struct ipaccess_entry *a = cfg_malloc(sizeof(struct ipaccess_entry));

  a->allow = is_allow;
  if (p)
    {
      *p++ = 0;
      if (q = parse_ip(p, &a->mask))
	return q;
    }
  else
    a->mask = ~0;
  add_tail(&l->l, &a->n);
  return parse_ip(c, &a->addr);
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
