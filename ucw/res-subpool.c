/*
 *	The UCW Library -- Resources for Sub-pools
 *
 *	(c) 2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/respool.h"

#include <stdio.h>

static void
subpool_res_free(struct resource *r)
{
  struct respool *rp = r->priv;
  rp->subpool_of = NULL;
  rp_delete(rp);
}

static void
subpool_res_detach(struct resource *r)
{
  struct respool *rp = r->priv;
  rp->subpool_of = NULL;
}

static void
subpool_res_dump(struct resource *r, uns indent)
{
  printf(":\n");
  rp_dump(r->priv, indent);
}

static const struct res_class subpool_res_class = {
  .name = "subpool",
  .dump = subpool_res_dump,
  .detach = subpool_res_detach,
  .free = subpool_res_free,
};

struct resource *
res_subpool(struct respool *rp)
{
  ASSERT(!rp->subpool_of);
  struct resource *r = res_new(&subpool_res_class, rp);
  ASSERT(r);
  ASSERT(r->rpool != rp);		// Avoid simple loops
  rp->subpool_of = r;
  return r;
}

#ifdef TEST

int main(void)
{
  struct respool *rp = rp_new("interior", NULL);
  struct respool *rp2 = rp_new("exterior", NULL);
  rp_switch(rp);
  res_malloc(10, NULL);
  rp_switch(rp2);
  res_malloc(7, NULL);
  res_subpool(rp);
  rp_dump(rp2, 0);
  // rp_delete(rp);
  // rp_dump(rp2, 0);
  rp_delete(rp2);
  return 0;
}

#endif
