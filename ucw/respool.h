/*
 *	The UCW Library -- Resource Pools
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 * FIXME:
 *	- check other candidates for resourcification
 *	- respool as a resource in another respool?
 *	- unit tests
 *	- automatic freeing of trans pool on thread exit
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
  const struct res_class *rclass;
  void *priv;						// Private to the class
};

struct res_class {
  const char *name;
  void (*detach)(struct resource *r);
  void (*free)(struct resource *r);
  void (*dump)(struct resource *r);
  uns res_size;						// Size of the resource structure (0=default)
};

struct respool *rp_new(const char *name, struct mempool *mp);
void rp_delete(struct respool *rp);
void rp_detach(struct respool *rp);
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

struct resource *res_alloc(const struct res_class *rc);	// Returns NULL if there is no pool active
void res_drop(struct resource *r);
void res_detach(struct resource *r);
void res_free(struct resource *r);
void res_dump(struct resource *r);

static inline struct resource *				// Returns NULL if there is no pool active
res_new(const struct res_class *rc, void *priv)
{
  struct resource *r = res_alloc(rc);
  if (r)
    {
      r->rclass = rc;
      r->priv = priv;
    }
  return r;
}

/* Various special types of resources */

struct resource *res_for_fd(int fd);			// Creates a resource that closes a given file descriptor

void *res_malloc(size_t size, struct resource **ptr);	// Allocates memory and creates a resource for it
void *res_malloc_zero(size_t size, struct resource **ptr);	// Allocates zero-initialized memory and creates a resource for it
void *res_realloc(struct resource *res, size_t size);

#endif
