/*
 *	UCW Library -- Memory Pools (One-Time Allocation)
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/mempool.h"

#include <string.h>

struct memchunk {
  struct memchunk *next;
  byte data[0];
};

struct mempool *
mp_new(uns size)
{
  struct mempool *p = xmalloc(sizeof(struct mempool));

  size -= sizeof(struct memchunk);
  p->free = p->last = NULL;
  p->first = p->current = p->first_large = NULL;
  p->plast = &p->first;
  p->chunk_size = size;
  p->threshold = size / 3;
  return p;
}

void
mp_delete(struct mempool *p)
{
  struct memchunk *c, *d;

  for(d=p->first; d; d = c)
    {
      c = d->next;
      xfree(d);
    }
  for(d=p->first_large; d; d = c)
    {
      c = d->next;
      xfree(d);
    }
  xfree(p);
}

void
mp_flush(struct mempool *p)
{
  struct memchunk *c;

  p->free = p->last = NULL;
  p->current = p->first;
  while (c = p->first_large)
    {
      p->first_large = c->next;
      xfree(c);
    }
}

void *
mp_alloc(struct mempool *p, uns s)
{
  if (s <= p->threshold)
    {
      byte *x = (byte *)(((addr_int_t) p->free + POOL_ALIGN - 1) & ~(addr_int_t)(POOL_ALIGN - 1));
      if (x + s > p->last)
	{
	  struct memchunk *c;

	  if (p->current)
	    {
	      /* Still have free chunks from previous incarnation */
	      c = p->current;
	      p->current = c->next;
	    }
	  else
	    {
	      c = xmalloc(sizeof(struct memchunk) + p->chunk_size);
	      *p->plast = c;
	      p->plast = &c->next;
	      c->next = NULL;
	    }
	  x = c->data;
	  p->last = x + p->chunk_size;
	}
      p->free = x + s;
      return x;
    }
  else
    {
      struct memchunk *c = xmalloc(sizeof(struct memchunk) + s);
      c->next = p->first_large;
      p->first_large = c;
      return c->data;
    }
}

void *
mp_alloc_zero(struct mempool *p, uns s)
{
  void *x = mp_alloc(p, s);
  bzero(x, s);
  return x;
}
