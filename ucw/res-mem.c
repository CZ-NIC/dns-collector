/*
 *	The UCW Library -- Resources for Memory Blocks
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/resource.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct res_mem {
  struct resource r;
  size_t size;			// Just for sake of readable resource dumps
};

static void
mem_res_free(struct resource *r)
{
  xfree(r->priv);
}

static void
mem_res_dump(struct resource *r, uint indent UNUSED)
{
  struct res_mem *rm = (struct res_mem *) r;
  printf(" size=%zu, ptr=%p\n", rm->size, r->priv);
}

static const struct res_class mem_res_class = {
  .name = "mem",
  .dump = mem_res_dump,
  .free = mem_res_free,
  .res_size = sizeof(struct res_mem),
};

void *
res_malloc(size_t size, struct resource **ptr)
{
  void *p = xmalloc(size);
  struct resource *r = res_new(&mem_res_class, p);
  ((struct res_mem *) r) -> size = size;
  if (ptr)
    *ptr = r;
  return p;
}

void *
res_malloc_zero(size_t size, struct resource **ptr)
{
  void *p = res_malloc(size, ptr);
  bzero(p, size);
  return p;
}

void *
res_realloc(struct resource *r, size_t size)
{
  struct res_mem *rm = (struct res_mem *) r;
  r->priv = xrealloc(r->priv, size);
  rm->size = size;
  return r->priv;
}

#ifdef TEST

int main(void)
{
  struct respool *rp = rp_new("test", NULL);
  rp_switch(rp);
  struct resource *r;
  char *p = res_malloc(3, &r);
  p[0] = p[1] = p[2] = 1;
  rp_dump(rp, 0);
  p = res_realloc(r, 5);
  p[3] = p[4] = 2;
  rp_dump(rp, 0);
  rp_delete(rp);
  return 0;
}

#endif
