/*
 *	Sherlock Library -- Object Functions
 *
 *	(c) 1997--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/pools.h"
#include "lib/fastbuf.h"

#include <string.h>
#include <stdio.h>

#define OBJ_POOL_SIZE 4096

void
obj_dump(struct odes *o)
{
  struct oattr *a, *b;

  for(a=o->attrs; a; a=a->next)
    for(b=a; b; b=b->same)
      printf("%c%s\n", (a==b ? a->attr : ' '), b->val);
}

static struct oattr *
oa_new(struct odes *o, uns x, byte *v)
{
  struct oattr *a = mp_alloc(o->pool, sizeof(struct oattr) + strlen(v));

  a->next = a->same = NULL;
  a->attr = x;
  strcpy(a->val, v);
  return a;
}

struct odes *
obj_new(struct mempool *pool)
{
  struct odes *o;

  o = mp_alloc(pool, sizeof(struct odes));
  o->pool = pool;
  o->attrs = NULL;
  o->cached_attr = NULL;
  return o;
}

int
obj_read(struct fastbuf *f, struct odes *o)
{
  byte buf[1024];

  while (bgets(f, buf, sizeof(buf)))
    {
      if (!buf[0])
	return 1;
      obj_add_attr(o, buf[0], buf+1);
    }
  return 0;
}

void
obj_write(struct fastbuf *f, struct odes *d)
{
  struct oattr *a, *b;
  byte *z;

  for(a=d->attrs; a; a=a->next)
    for(b=a; b; b=b->same)
      {
	bputc(f, a->attr);
	for(z = b->val; *z; z++)
	  if (*z >= ' ' || *z == '\t')
	    bputc(f, *z);
	  else
	    {
	      bputc(f, '?');
	      log(L_ERROR, "obj_dump: Found non-ASCII characters (URL might be %s)", obj_find_aval(d, 'U'));
	    }
	bputc(f, '\n');
      }
}

struct oattr *
obj_find_attr(struct odes *o, uns x)
{
  struct oattr *a;

  for(a=o->attrs; a && a->attr != x; a=a->next)
    ;
  return a;
}

struct oattr *
obj_find_attr_last(struct odes *o, uns x)
{
  struct oattr *a = obj_find_attr(o, x);

  if (a)
    {
      while (a->same)
	a = a->same;
    }
  return a;
}

uns
obj_del_attr(struct odes *o, struct oattr *a)
{
  struct oattr *x, **p, *y, *l;
  byte aa = a->attr;

  o->cached_attr = NULL;
  p = &o->attrs;
  while (x = *p)
    {
      if (x->attr == aa)
	{
	  y = x;
	  l = NULL;
	  while (x = *p)
	    {
	      if (x == a)
		{
		  *p = x->same;
		  return 1;
		}
	      p = &x->same;
	      l = x;
	    }
	  return 0;
	}
      p = &x->next;
    }
  return 0;
}

byte *
obj_find_aval(struct odes *o, uns x)
{
  struct oattr *a = obj_find_attr(o, x);

  return a ? a->val : NULL;
}

struct oattr *
obj_set_attr(struct odes *o, uns x, byte *v)
{
  struct oattr *a, **z;

  z = &o->attrs;
  while (a = *z)
    {
      if (a->attr == x)
	{
	  *z = a->next;
	  goto set;
	}
      z = &a->next;
    }

 set:
  if (v)
    {
      a = oa_new(o, x, v);
      a->next = o->attrs;
      o->attrs = a;
    }
  else
    a = NULL;
  o->cached_attr = a;
  return a;
}

struct oattr *
obj_set_attr_num(struct odes *o, uns a, uns v)
{
  byte x[32];

  sprintf(x, "%d", v);
  return obj_set_attr(o, a, x);
}

struct oattr *
obj_add_attr(struct odes *o, uns x, byte *v)
{
  struct oattr *a, *b;

  b = oa_new(o, x, v);
  if (!(a = o->cached_attr) || a->attr != x)
    {
      if (!(a = obj_find_attr(o, x)))
	{
	  b->next = o->attrs;
	  o->attrs = b;
	  goto done;
	}
    }
  while (a->same)
    a = a->same;
  a->same = b;
 done:
  o->cached_attr = b;
  return b;
}

struct oattr *
obj_prepend_attr(struct odes *o, uns x, byte *v)
{
  struct oattr *a, *b, **z;

  b = oa_new(o, x, v);
  z = &o->attrs;
  while (a = *z)
    {
      if (a->attr == x)
	{
	  b->same = a;
	  b->next = a->next;
	  a->next = NULL;
	  *z = b;
	  return b;
	}
      z = &a->next;
    }
  b->next = o->attrs;
  o->attrs = b;
  return b;
}

struct oattr *
obj_insert_attr(struct odes *o, struct oattr *first, struct oattr *after, byte *v)
{
  struct oattr *b;

  b = oa_new(o, first->attr, v);
  b->same = after->same;
  after->same = b;
  return b;
}
