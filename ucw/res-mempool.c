/*
 *	The UCW Library -- Resources for Memory Pools
 *
 *	(c) 2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/resource.h"
#include "ucw/mempool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
mp_res_free(struct resource *r)
{
  mp_delete(r->priv);
}

static void
mp_res_dump(struct resource *r, uns indent UNUSED)
{
  printf(" pool=%p\n", r->priv);
}

static const struct res_class mp_res_class = {
  .name = "mempool",
  .dump = mp_res_dump,
  .free = mp_res_free,
};

struct resource *
res_mempool(struct mempool *mp)
{
  return res_new(&mp_res_class, mp);
}

#ifdef TEST

int main(void)
{
  struct respool *rp = rp_new("test", NULL);
  rp_switch(rp);
  res_mempool(mp_new(4096));
  rp_dump(rp, 0);
  rp_delete(rp);
  return 0;
}

#endif
