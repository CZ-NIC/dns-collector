/*
 *	The UCW Library -- Resources for Memory Pools
 *
 *	(c) 2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/respool.h"
#include "ucw/eltpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
ep_res_free(struct resource *r)
{
  ep_delete(r->priv);
}

static void
ep_res_dump(struct resource *r, uns indent UNUSED)
{
  printf(" pool=%p\n", r->priv);
}

static const struct res_class ep_res_class = {
  .name = "eltpool",
  .dump = ep_res_dump,
  .free = ep_res_free,
};

struct resource *
res_eltpool(struct eltpool *mp)
{
  return res_new(&ep_res_class, mp);
}

#ifdef TEST

int main(void)
{
  struct respool *rp = rp_new("test", NULL);
  rp_switch(rp);
  res_eltpool(ep_new(16, 256));
  rp_dump(rp, 0);
  rp_delete(rp);
  return 0;
}

#endif
