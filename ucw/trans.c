/*
 *	The UCW Library -- Transactions
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define LOCAL_DEBUG

#include "ucw/lib.h"
#include "ucw/trans.h"
#include "ucw/respool.h"
#include "ucw/mempool.h"

#include <stdio.h>

void
trans_init(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  if (!c->trans_pool)
    c->trans_pool = mp_new(1024);
}

void
trans_cleanup(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  if (c->trans_pool)
    {
      mp_delete(c->trans_pool);
      c->trans_pool = NULL;
    }
  c->current_trans = NULL;
}

struct trans *
trans_open_rp(struct respool *rp)
{
  trans_init();
  struct ucwlib_context *c = ucwlib_thread_context();
  struct mempool *mp = c->trans_pool;

  struct mempool_state *mst = mp_push(mp);
  struct trans *t = mp_alloc(mp, sizeof(*t));
  t->trans_pool_state = mst;

  if (!rp)
    rp = rp_new("trans", mp);
  t->rpool = rp;
  t->prev_rpool = rp_switch(rp);

  t->prev_trans = c->current_trans;
  c->current_trans = t;
  DBG("Opened transaction %p", t);
  return t;
}

struct trans *
trans_get_current(void)
{
  return ucwlib_thread_context() -> current_trans;
}

static void
trans_close(struct trans *t)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  rp_switch(t->prev_rpool);
  c->current_trans = t->prev_trans;
  mp_restore(c->trans_pool, t->trans_pool_state);
}

void
trans_commit(void)
{
  struct trans *t = trans_get_current();
  DBG("Commiting transaction %p", t);
  ASSERT(t);
  rp_detach(t->rpool);
  trans_close(t);
}

void
trans_rollback(void)
{
  struct trans *t = trans_get_current();
  DBG("Rolling back transaction %p", t);
  ASSERT(t);
  rp_delete(t->rpool);
  trans_close(t);
}

void
trans_dump(void)
{
  struct trans *t = trans_get_current();
  if (!t)
    {
      puts("No transaction open.");
      return;
    }
  while (t)
    {
      printf("Transaction %p:\n", t);
      rp_dump(t->rpool);
      t = t->prev_trans;
    }
}

#ifdef TEST

int main(void)
{
  trans_open();
  res_malloc(64, NULL);
  trans_dump();
  trans_commit();
  trans_cleanup();
  return 0;
}

#endif
