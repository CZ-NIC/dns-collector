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

#include <ucw/clists.h>
#include <ucw/threads.h>

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
  uns default_res_flags;				// RES_FLAG_xxx for newly allocated resources
};

/**
 * Each resource is represented by this structure. It is linked to a resource
 * pool it belongs to. It contains a pointer to a resource class (which describes how to
 * handle the resource) and data private to the resource class.
 **/
struct resource {
  cnode n;
  struct respool *rpool;
  uns flags;						// RES_FLAG_xxx
  const struct res_class *rclass;
  void *priv;						// Private to the class
  // More data specific for the particular class can follow
};

/** Resource flags **/
enum resource_flags {
  RES_FLAG_TEMP = 1,					// Resource is temporary
  RES_FLAG_XFREE = 2,					// Resource structure needs to be deallocated by xfree()
};

/**
 * Creates a new resource pool. If a memory pool is given, meta-data of all resources
 * will be allocated from this pool. Otherwise, they will be malloc'ed.
 **/
struct respool *rp_new(const char *name, struct mempool *mp);

void rp_delete(struct respool *rp);			/** Deletes a resource pool, freeing all resources. **/
void rp_detach(struct respool *rp);			/** Deletes a resource pool, detaching all resources. **/
void rp_commit(struct respool *rp);			/** Deletes a resource pool. Temporary resources are freed, stable resources are detached. **/
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

/**
 * Frees a resource, unlinking it from its pool.
 * When called with a NULL pointer, it does nothing, but safely.
 **/
void res_free(struct resource *r);

/**
 * Unlinks a resource from a pool and releases its meta-data. However, the resource itself is kept.
 * When called with a NULL pointer, it does nothing, but safely.
 **/
void res_detach(struct resource *r);

/** Marks a resource as temporary (sets @RES_FLAG_TEMP). **/
static inline void res_temporary(struct resource *r)
{
  r->flags |= RES_FLAG_TEMP;
}

/** Marks a resource as permanent (clears @RES_FLAG_TEMP). **/
static inline void res_permanent(struct resource *r)
{
  r->flags &= RES_FLAG_TEMP;
}

/***
 * === Resource classes
 *
 * A resource class describes how to handle a particular type of resources.
 * Most importantly, it defines a set of (optional) callbacks for performing operations
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

/**
 * Initialize a pre-allocated buffer to the specific class of resource, setting its private data to @priv.
 * This resource can be added to the current pool by @res_add().
 **/
static inline struct resource *res_init(struct resource *r, const struct res_class *rc, void *priv)
{
  r->flags = 0;
  r->rclass = rc;
  r->priv = priv;
  return r;
}

/**
 * Links a pre-initialized resource to the active pool.
 **/
void res_add(struct resource *r);

/**
 * Unlinks a resource from a pool and releases its meta-data. Unlike @res_detach(),
 * it does not invoke any callbacks. The caller must make sure that no references to
 * the meta-data remain, so this is generally safe only inside resource class code.
 **/
void res_drop(struct resource *r);

/**
 * Creates a new resource of the specific class, setting its private data to @priv.
 * Dies if no resource pool is active.
 **/
static inline struct resource *res_new(const struct res_class *rc, void *priv)
{
  struct resource *r = res_alloc(rc);
  r->rclass = rc;
  r->priv = priv;
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

struct mempool;
struct resource *res_mempool(struct mempool *mp);			/** Creates a resource for the specified <<mempool:,memory pool>>. **/

struct eltpool;
struct resource *res_eltpool(struct eltpool *ep);			/** Creates a resource for the specified <<eltpool:,element pool>>. **/

#endif
