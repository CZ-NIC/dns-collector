/*
 *	The UCW Library -- Transactions
 *
 *	(c) 2008--2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

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

  t->thrown_exc = NULL;
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
  ASSERT(!t->thrown_exc);
  rp_detach(t->rpool);
  trans_pop(t, c);
  trans_drop(t, c);
}

void
trans_rollback(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  struct trans *t = c->current_trans;
  DBG("Rolling back transaction %p", t);
  ASSERT(t);
  ASSERT(!t->thrown_exc);
  rp_delete(t->rpool);
  trans_pop(t, c);
  trans_drop(t, c);
}

void
trans_fold(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  struct trans *t = c->current_trans;
  DBG("Folding transaction %p", t);
  ASSERT(t);
  ASSERT(!t->thrown_exc);
  trans_pop(t, c);
  ASSERT(c->current_trans);	// Ensure that the parent exists
  res_subpool(t->rpool);
  t->rpool = NULL;		// To stop people from using the trans
}

void
trans_caught(void)
{
  // Exception has been finally caught. Roll back the current transaction,
  // including all sub-transactions that have been folded to it during
  // propagation.
  struct trans *t = trans_get_current();
  struct exception *x = t->thrown_exc;
  ASSERT(x);
  DBG("... exception %p caught", x);
  t->thrown_exc = NULL;
  trans_rollback();
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
      struct exception *x = t->thrown_exc;
      if (x)
	printf("    Exception %s (%s) in flight\n", x->id, x->msg);
      rp_dump(t->rpool, 4);
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

  // If we are already handling an exception (i.e., throw from a catch handler),
  // fold the current transaction into its parent.
  while (t->thrown_exc)
    {
      if (!t->prev_trans)
	die("%s", x->msg);
      t->thrown_exc = NULL;
      trans_fold();
      t = c->current_trans;
      DBG("... recursive throw propagated to parent trans %p", t);
    }

  // And jump overboard.
  t->thrown_exc = x;
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
  return trans_get_current() -> thrown_exc;
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
	  TRANS_TRY
	    {
	      res_malloc(256, NULL);
	      trans_throw("ucw.test.nested", "nest", "Something is wrong recursively");
	    }
	  TRANS_CATCH(y)
	    {
	      printf("Yet another layer: %s\n", y->msg);
	      trans_dump();
	      // trans_throw_exc(y);
	    }
	  TRANS_END;
	  trans_throw("ucw.test2", "out", "Error: %s", x->msg);
	}
      TRANS_END;
    }
  TRANS_CATCH(x)
    {
      printf("Outer catch: %s\n", x->msg);
      trans_dump();
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
