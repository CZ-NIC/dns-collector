/*
 *	Sherlock Library -- Object Functions
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
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
  a->last_same = a;
  a->attr = x;
  strcpy(a->val, v);
  return a;
}

struct odes *
obj_new(struct mempool *pool)
{
  struct mempool *lp = pool;
  struct odes *o;

  if (!lp)
    lp = mp_new(OBJ_POOL_SIZE);
  o = mp_alloc(lp, sizeof(struct odes));
  o->pool = lp;
  o->local_pool = (pool == lp) ? NULL : lp;
  o->attrs = NULL;
  return o;
}

void
obj_free(struct odes *o)
{
  if (o->local_pool)
    mp_delete(o->local_pool);
}

int
obj_read(struct fastbuf *f, struct odes *o)
{
  byte buf[4096];
  struct oattr **last = &o->attrs;
  struct oattr *a, *la;

  la = NULL;
  *last = NULL;
  while (bgets(f, buf, sizeof(buf)))
    {
      if (!buf[0])
	return 1;
      a = oa_new(o, buf[0], buf+1);
      if (!la || la->attr != a->attr)
	for(la=o->attrs; la && la->attr != a->attr; la=la->next)
	  ;
      if (la)
	{
	  la->last_same->same = a;
	  la->last_same = a;
	}
      else
	{
	  *last = a;
	  last = &a->next;
	  la = a;
	}
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
	  if (*z >= ' ')
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

  return a ? a->last_same : NULL;
}

uns
obj_del_attr(struct odes *o, struct oattr *a)
{
  struct oattr *x, **p, *y, *l;
  byte aa = a->attr;

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
		  if (y->last_same == x)
		    y->last_same = l;
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
obj_add_attr(struct odes *o, struct oattr *a, uns x, byte *v)
{
  struct oattr *b;

  if (!a)
    {
      a = obj_find_attr(o, x);
      if (!a)
	return obj_set_attr(o, x, v);
    }
  b = oa_new(o, x, v);
  a->last_same->same = b;
  a->last_same = b;
  return a;
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
	  b->last_same = a->last_same;
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
  if (first->last_same == after)
    first->last_same = b;
  return b;
}
