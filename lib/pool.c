/*
 *	Sherlock Library -- Memory Pools (One-Time Allocation)
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>

#include "lib.h"
#include "pools.h"

struct memchunk {
  struct memchunk *next;
  byte data[0];
};

struct mempool *
new_pool(uns size)
{
  struct mempool *p = xmalloc(sizeof(struct mempool));

  size -= sizeof(struct memchunk);
  p->chunks = NULL;
  p->free = p->last = NULL;
  p->chunk_size = size;
  p->threshold = size / 3;
  return p;
}

void
free_pool(struct mempool *p)
{
  struct memchunk *c = p->chunks;

  while (c)
    {
      struct memchunk *n = c->next;
      free(c);
      c = n;
    }
  free(p);
}

void *
pool_alloc(struct mempool *p, uns s)
{
  if (s <= p->threshold)
    {
      byte *x = (byte *)(((uns) p->free + POOL_ALIGN - 1) & ~(POOL_ALIGN - 1));
      if (x + s > p->last)
	{
	  struct memchunk *c = xmalloc(sizeof(struct memchunk) + p->chunk_size);
	  c->next = p->chunks;
	  p->chunks = c;
	  x = c->data;
	  p->last = x + p->chunk_size;
	}
      p->free = x + s;
      return x;
    }
  else
    {
      struct memchunk *c = xmalloc(sizeof(struct memchunk) + s);
      c->next = p->chunks;
      p->chunks = c;
      return c->data;
    }
}
