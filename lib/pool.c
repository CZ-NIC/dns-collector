/*
 *	Sherlock Library -- Memory Pools (One-Time Allocation)
 *
 *	(c) 1997--1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>

#include "lib/lib.h"
#include "lib/pools.h"

struct memchunk {
  struct memchunk *next;
  byte data[0];
};

struct mempool *
new_pool(uns size)
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
free_pool(struct mempool *p)
{
  struct memchunk *c, *d;

  for(d=p->first; d; d = c)
    {
      c = d->next;
      free(d);
    }
  for(d=p->first_large; d; d = c)
    {
      c = d->next;
      free(d);
    }
  free(p);
}

void
flush_pool(struct mempool *p)
{
  struct memchunk *c;

  p->free = p->last = NULL;
  p->current = p->first;
  while (c = p->first_large)
    {
      p->first_large = c->next;
      free(c);
    }
}

void *
pool_alloc(struct mempool *p, uns s)
{
  if (s <= p->threshold)
    {
      byte *x = (byte *)(((uns) p->free + POOL_ALIGN - 1) & ~(POOL_ALIGN - 1));
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
