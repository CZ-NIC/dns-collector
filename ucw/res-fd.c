/*
 *	The UCW Library -- Resources for File Descriptors
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/respool.h"

#include <stdio.h>
#include <unistd.h>

static void
fd_res_free(struct resource *r)
{
  close((int)(intptr_t) r->priv);
}

static void
fd_res_dump(struct resource *r, uns indent UNUSED)
{
  printf(" fd=%d\n", (int)(intptr_t) r->priv);
}

static const struct res_class fd_res_class = {
  .name = "fd",
  .dump = fd_res_dump,
  .free = fd_res_free,
};

struct resource *
res_for_fd(int fd)
{
  return res_new(&fd_res_class, (void*)(intptr_t) fd);
}

#ifdef TEST

int main(void)
{
  struct respool *rp = rp_new("test", NULL);
  rp_switch(rp);
  res_for_fd(1);
  rp_dump(rp, 0);
  rp_delete(rp);
  return 0;
}

#endif
