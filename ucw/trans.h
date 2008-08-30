/*
 *	The UCW Library -- Transactions
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_TRANS_H
#define _UCW_TRANS_H

#include "ucw/clists.h"
#include "ucw/mempool.h"

#include <jmpbuf.h>

/* Resource pools */

struct respool {
  clist resources;
  struct mempool *mp;		// If set, resources are allocated from the mempool, otherwise by xmalloc()
};

struct resource {
  cnode n;
  struct res_class *rclass;
  void *priv;			// Private to the class
};

struct res_class {
  const char *name;
  void (*undo)(struct res *t);
  void (*dump)(struct res *t);
};

struct respool *rp_new(void);
void rp_delete(struct respool *rp);
void rp_dump(struct respool *rp);

struct resource *res_alloc(struct respool *rp);
void res_fix(struct resource *r);
void res_undo(struct resource *r);
void res_dump(struct resource *r);

static inline struct resource *
res_new(struct respool *rp, struct res_class *rc, void *priv)
{
  struct resource *r = res_alloc(rp);
  r->rclass = rc;
  r->priv = priv;
  return r;
}

/* Transactions */

struct trans {
  struct trans *prev;
  struct mempool_state trans_pool_state;
  struct respool res_pool;
  jmp_buf jmp;
};

void trans_init(void);		// Called automatically on trans_open() if needed
void trans_cleanup(void);	// Free memory occupied by the transaction system pools

struct trans *trans_open(void);
struct trans *trans_get_current(void);
void trans_commit(void);
void trans_rollback(void);
void trans_dump(void);

/* Exceptions */

#endif
