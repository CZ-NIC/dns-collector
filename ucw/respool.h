/*
 *	The UCW Library -- Resource Pools
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_RESPOOL_H
#define _UCW_RESPOOL_H

#include "ucw/clists.h"
#include "ucw/threads.h"

struct respool {
  clist resources;
  const char *name;
  struct mempool *mpool;				// If set, resources are allocated from the mempool, otherwise by xmalloc()
};

struct resource {
  cnode n;
  struct respool *rpool;
  struct res_class *rclass;
  void *priv;						// Private to the class
};

struct res_class {
  const char *name;
  void (*detach)(struct resource *r);
  void (*free)(struct resource *r);
  void (*dump)(struct resource *r);
};

struct respool *rp_new(const char *name, struct mempool *mp);
void rp_delete(struct respool *rp);
void rp_dump(struct respool *rp);

static inline struct respool *
rp_current(void)
{
  return ucwlib_thread_context()->current_respool;	// May be NULL
}

static inline struct respool *
rp_switch(struct respool *rp)
{
  struct ucwlib_context *ctx = ucwlib_thread_context();
  struct respool *orp = ctx->current_respool;
  ctx->current_respool = rp;
  return orp;
}

struct resource *res_alloc(void);			// Returns NULL if there is no pool active
void res_detach(struct resource *r);
void res_free(struct resource *r);
void res_dump(struct resource *r);

static inline struct resource *				// Returns NULL if there is no pool active
res_new(struct res_class *rc, void *priv)
{
  struct resource *r = res_alloc();
  if (r)
    {
      r->rclass = rc;
      r->priv = priv;
    }
  return r;
}

#endif
