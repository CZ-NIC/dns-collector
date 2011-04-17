/*
 *	The UCW Library -- Resource Pools
 *
 *	(c) 2008--2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_RESPOOL_H
#define _UCW_RESPOOL_H

#include "ucw/clists.h"
#include "ucw/threads.h"

/**
 * A resource pool. It contains a name of the pool (which is printed
 * in all debugging dumps, otherwise it is not used) and a bunch of
 * fields for internal use.
 **/
struct respool {
  clist resources;
  const char *name;
  struct mempool *mpool;				// If set, resources are allocated from the mempool, otherwise by xmalloc()
  struct resource *subpool_of;
};

/**
 * Each resource is represented by this structure. It is linked to a resource
 * pool it belongs to. It contains a pointer to a resource class (which describes how to
 * handle the resource) and data private to the resource class.
 **/
struct resource {
  cnode n;
  struct respool *rpool;
  const struct res_class *rclass;
  void *priv;						// Private to the class
  // More data specific for the particular class can follow
};

/**
 * Creates a new resource pool. If a memory pool is given, meta-data of all resources
 * will be allocated from this pool. Otherwise, they will be malloc'ed.
 **/
struct respool *rp_new(const char *name, struct mempool *mp);

void rp_delete(struct respool *rp);			/** Deletes a resource pool, freeing all resources. **/
void rp_detach(struct respool *rp);			/** Deletes a resource pool, detaching all resources. **/
void rp_dump(struct respool *rp, uns indent);		/** Prints out a debugging dump of a pool to stdout. **/

/** Returns a pointer to the currently active resource pool or NULL, if none exists. **/
static inline struct respool *rp_current(void)
{
  return ucwlib_thread_context()->current_respool;
}

/**
 * Makes the given resource pool active; returns a pointer to the previously active pool
 * or NULL, if there was none. Calling with @rp equal to NULL deactivates the pool.
 **/
static inline struct respool *rp_switch(struct respool *rp)
{
  struct ucwlib_context *ctx = ucwlib_thread_context();
  struct respool *orp = ctx->current_respool;
  ctx->current_respool = rp;
  return orp;
}

struct resource *res_alloc(const struct res_class *rc) LIKE_MALLOC;	// Returns NULL if there is no pool active

void res_dump(struct resource *r, uns indent);		/** Prints out a debugging dump of the resource to stdout. **/
void res_free(struct resource *r);			/** Frees a resource, unlinking it from its pool. **/

/***
 * === Resource classes
 *
 * A resource class describes how to handle a particular type of resources.
 * Most importantly, it defines a set of callbacks for performing operations
 * on the resources:
 *
 * * dump() should print a description of the resource used for debugging
 *   to the standard output. The description should end with a newline character
 *   and in case of a multi-line description, the subsequent lines should be
 *   indented by @indent spaces.
 * * free() frees the resource; the struct resource is freed automatically afterwards.
 * * detach() breaks the link between the struct resource and the real resource;
 *   the struct resource is freed automatically afterwards, while the resource
 *   continues to live.
 *
 * The following functions are intended for use by the resource classes only.
 ***/

/** The structure describing a resource class. **/
struct res_class {
  const char *name;					// The name of the class (included in debugging dumps)
  void (*detach)(struct resource *r);			// The callbacks
  void (*free)(struct resource *r);
  void (*dump)(struct resource *r, uns indent);
  uns res_size;						// Size of the resource structure (0=default)
};

/** Unlinks a resource from a pool and releases its meta-data. However, the resource itself is kept. **/
void res_detach(struct resource *r);

/**
 * Unlinks a resource from a pool and releases its meta-data. Unlike @res_detach(),
 * it does not invoke any callbacks.
 **/
void res_drop(struct resource *r);

/**
 * Creates a new resource of the specific class, setting its private data to @priv.
 * Returns NULL if there is no resource pool active.
 **/
static inline struct resource *res_new(const struct res_class *rc, void *priv)
{
  struct resource *r = res_alloc(rc);
  if (r)
    {
      r->rclass = rc;
      r->priv = priv;
    }
  return r;
}

/***
 * === Pre-defined resource classes
 ***/

struct resource *res_for_fd(int fd);			/** Creates a resource that closes a given file descriptor. **/

void *res_malloc(size_t size, struct resource **ptr) LIKE_MALLOC;	/** Allocates memory and creates a resource for it. **/
void *res_malloc_zero(size_t size, struct resource **ptr) LIKE_MALLOC;	/** Allocates zero-initialized memory and creates a resource for it. **/
void *res_realloc(struct resource *res, size_t size);			/** Re-allocates memory obtained by @res_malloc() or @res_malloc_zero(). **/

/**
 * Converts the resource pool @rp to a resource inside the current resource pool (i.e., its sub-pool).
 * You can delete the sub-pool either by freeing this resource, or by calling
 * @rp_delete() on it, which removes the resource automatically.
 **/
struct resource *res_subpool(struct respool *rp);

#endif
