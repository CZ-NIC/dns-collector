/*
 *	The UCW Library -- Resource Pools
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/respool.h"
#include "ucw/mempool.h"

#include <stdio.h>

struct respool *
rp_new(const char *name, struct mempool *mp)
{
  struct respool *rp;

  if (mp)
    {
      rp = mp_alloc_zero(mp, sizeof(*rp));
      rp->mpool = mp;
    }
  else
    rp = xmalloc_zero(sizeof(*rp));
  clist_init(&rp->resources);
  rp->name = name;
  return rp;
}

static void
rp_free(struct respool *rp)
{
  if (!rp->mpool)
    xfree(rp);
  if (rp_current() == rp)
    rp_switch(NULL);
}

void
rp_delete(struct respool *rp)
{
  struct resource *r;
  while (r = clist_tail(&rp->resources))
    {
      ASSERT(r->rpool == rp);
      res_free(r);
    }
  rp_free(rp);
}

void
rp_detach(struct respool *rp)
{
  struct resource *r;
  while (r = clist_head(&rp->resources))
    {
      ASSERT(r->rpool == rp);
      res_detach(r);
    }
  rp_free(rp);
}

void
rp_dump(struct respool *rp)
{
  printf("Resource pool %s at %p (%s):\n", (rp->name ? : "(noname)"), rp, (rp->mpool ? "mempool-based" : "freestanding"));
  CLIST_FOR_EACH(struct resource *, r, rp->resources)
    res_dump(r);
}

struct resource *
res_alloc(const struct res_class *rc)
{
  struct respool *rp = rp_current();
  if (!rp)
    return NULL;

  uns size = (rc->res_size ? : sizeof(struct resource));
  struct resource *r = (rp->mpool ? mp_alloc_fast(rp->mpool, size) : xmalloc(size));
  r->rpool = rp;
  clist_add_tail(&rp->resources, &r->n);
  return r;
}

void
res_drop(struct resource *r)
{
  clist_remove(&r->n);
  if (!r->rpool->mpool)
    xfree(r);
}

void
res_detach(struct resource *r)
{
  if (r->rclass->detach)
    r->rclass->detach(r);
  res_drop(r);
}

void
res_free(struct resource *r)
{
  if (r->rclass->free)
    r->rclass->free(r);
  res_drop(r);
}

void
res_dump(struct resource *r)
{
  printf("\t%p %s", r, r->rclass->name);
  if (r->rclass->dump)
    r->rclass->dump(r);
  putchar('\n');
}

#ifdef TEST

#include "ucw/fastbuf.h"

int main(void)
{
  // struct mempool *mp = mp_new(4096);
  struct respool *rp = rp_new("test", NULL);
  rp_switch(rp);
  struct fastbuf *f = bfdopen_shared(1, 0);
  rp_dump(rp);
  bputsn(f, "Hello, all worlds!");
  bclose(f);
  rp_delete(rp);
  return 0;
}

#endif
