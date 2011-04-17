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
  if (!c->exc_pool)
    c->exc_pool = mp_new(1024);
}

void
trans_cleanup(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  if (c->trans_pool)
    {
      mp_delete(c->trans_pool);
      mp_delete(c->exc_pool);
      c->trans_pool = NULL;
      c->exc_pool = NULL;
    }
  c->current_trans = NULL;
}

struct trans *
trans_open(void)
{
  trans_init();
  struct ucwlib_context *c = ucwlib_thread_context();
  struct mempool *mp = c->trans_pool;

  struct mempool_state *mst = mp_push(mp);
  struct trans *t = mp_alloc(mp, sizeof(*t));
  t->trans_pool_state = mst;

  struct respool *rp = rp_new("trans", mp);
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

struct mempool *
trans_get_pool(void)
{
  return ucwlib_thread_context() -> trans_pool;
}

struct mempool *
trans_get_exc_pool(void)
{
  return ucwlib_thread_context() -> exc_pool;
}

static void
trans_pop(struct trans *t, struct ucwlib_context *c)
{
  DBG("... popping trans %p", t);
  rp_switch(t->prev_rpool);
  c->current_trans = t->prev_trans;
}

static void
trans_drop(struct trans *t, struct ucwlib_context *c)
{
  DBG("... dropping trans %p", t);
  mp_restore(c->trans_pool, t->trans_pool_state);
}

void
trans_commit(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  struct trans *t = c->current_trans;
  DBG("Committing transaction %p", t);
  ASSERT(t);
  ASSERT(!c->current_exc);
  rp_detach(t->rpool);
  trans_pop(t, c);
  trans_drop(t, c);
}

static void
trans_rollback_exc(struct ucwlib_context *c)
{
  // In case we were processing an exception, roll back all transactions
  // through which the exception has propagated.
  struct exception *x = c->current_exc;
  struct trans *t = x->trans;
  while (t != c->current_trans)
    {
      DBG("Rolling back transaction %p after exception", t);
      struct trans *tprev = t->prev_trans;
      rp_delete(t->rpool);
      trans_drop(t, c);
      t = tprev;
    }

  c->current_exc = NULL;
  mp_flush(c->exc_pool);
}

void
trans_rollback(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  if (c->current_exc)
    {
      trans_rollback_exc(c);
      return;
    }

  struct trans *t = c->current_trans;
  ASSERT(t);
  DBG("Rolling back transaction %p", t);
  rp_delete(t->rpool);
  trans_pop(t, c);
  trans_drop(t, c);
}

void
trans_dump(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  struct exception *x = c->current_exc;
  struct trans *t = c->current_trans;

  if (x)
    {
      printf("Exception %s (%s) in flight\n", x->id, x->msg);
      struct trans *tx = x->trans;
      while (tx != t)
	{
	  printf("Recovering transaction %p:\n", tx);
	  rp_dump(tx->rpool);
	  tx = tx->prev_trans;
	}
    }

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

void
trans_throw_exc(struct exception *x)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  struct trans *t = c->current_trans;
  DBG("Throwing exception %s (%s) in trans %p", x->id, x->msg, t);
  if (!t)
    die("%s", x->msg);

  // Remember which transaction have the exceptions started to propagate from
  if (c->current_exc)
    x->trans = c->current_exc->trans;
  else
    x->trans = t;

  // Pop the current transaction off the transaction stack, but do not roll it
  // back, it will be done when the exception stops propagating.
  trans_pop(t, c);

  // And jump overboard.
  c->current_exc = x;
  longjmp(t->jmp, 1);
}

void
trans_throw(const char *id, void *object, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  trans_vthrow(id, object, fmt, args);
}

void
trans_vthrow(const char *id, void *object, const char *fmt, va_list args)
{
  struct mempool *mp = trans_get_pool();
  if (!mp)
    vdie(fmt, args);
  struct exception *x = mp_alloc(mp, sizeof(*x));
  x->id = id;
  x->object = object;
  x->msg = mp_vprintf(mp, fmt, args);
  trans_throw_exc(x);
}

struct exception *
trans_current_exc(void)
{
  return ucwlib_thread_context() -> current_exc;
}

#ifdef TEST

static void trc_free(struct resource *r)
{
  printf("Freeing object #%d\n", (int)(intptr_t) r->priv);
}

static struct res_class trc = {
  .name = "test",
  .free = trc_free,
};

int main(void)
{
  TRANS_TRY
    {
      res_new(&trc, (void *)(intptr_t) 1);
      res_malloc(64, NULL);
      TRANS_TRY
	{
	  res_new(&trc, (void *)(intptr_t) 2);
	  res_malloc(128, NULL);
	  trans_throw("ucw.test", "inn", "Universe failure: %d+%d=%d", 1, 2, 4);
	}
      TRANS_CATCH(x)
	{
	  printf("Inner catch: %s\n", x->msg);
	  trans_dump();
	  //trans_throw("ucw.test2", "out", "Error: %s", x->msg);
	}
      TRANS_END;
    }
  TRANS_CATCH(x)
    {
      printf("Outer catch: %s\n", x->msg);
    }
  TRANS_END;
  trans_throw("ucw.test3", "glob", "Global error. Reboot globe.");
#if 0
  trans_open();
  res_malloc(64, NULL);
  trans_dump();
  trans_commit();
#endif
  trans_cleanup();
  return 0;
}

#endif
