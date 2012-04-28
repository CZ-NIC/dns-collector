/*
 *	UCW Library -- Configuration files: journaling
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/getopt.h>
#include <ucw/conf-internal.h>
#include <ucw/mempool.h>

#include <string.h>

struct old_pools {
  struct old_pools *prev;
  struct mempool *pool;
};				// link-list of older cf_pool's

struct cf_journal_item {
  struct cf_journal_item *prev;
  byte *ptr;
  uns len;
  byte copy[0];
};

void
cf_set_journalling(int enable)
{
  struct cf_context *cc = cf_get_context();
  ASSERT(!cc->journal);
  cc->need_journal = enable;
}

void
cf_journal_block(void *ptr, uns len)
{
  struct cf_context *cc = cf_get_context();
  if (!cc->need_journal)
    return;
  struct cf_journal_item *ji = cf_malloc(sizeof(struct cf_journal_item) + len);
  ji->prev = cc->journal;
  ji->ptr = ptr;
  ji->len = len;
  memcpy(ji->copy, ptr, len);
  cc->journal = ji;
}

void
cf_journal_swap(void)
  // swaps the contents of the memory and the journal, and reverses the list
{
  struct cf_context *cc = cf_get_context();
  struct cf_journal_item *curr, *prev, *next;
  for (next=NULL, curr=cc->journal; curr; next=curr, curr=prev)
  {
    prev = curr->prev;
    curr->prev = next;
    for (uns i=0; i<curr->len; i++)
    {
      byte x = curr->copy[i];
      curr->copy[i] = curr->ptr[i];
      curr->ptr[i] = x;
    }
  }
  cc->journal = next;
}

struct cf_journal_item *
cf_journal_new_transaction(uns new_pool)
{
  struct cf_context *cc = cf_get_context();
  if (new_pool)
    cc->pool = mp_new(1<<10);
  struct cf_journal_item *oldj = cc->journal;
  cc->journal = NULL;
  return oldj;
}

void
cf_journal_commit_transaction(uns new_pool, struct cf_journal_item *oldj)
{
  struct cf_context *cc = cf_get_context();
  if (new_pool)
  {
    struct old_pools *p = cf_malloc(sizeof(struct old_pools));
    p->prev = cc->pools;
    p->pool = cc->pool;
    cc->pools = p;
  }
  if (oldj)
  {
    struct cf_journal_item **j = &cc->journal;
    while (*j)
      j = &(*j)->prev;
    *j = oldj;
  }
}

void
cf_journal_rollback_transaction(uns new_pool, struct cf_journal_item *oldj)
{
  struct cf_context *cc = cf_get_context();
  if (!cc->need_journal)
    die("Cannot rollback the configuration, because the journal is disabled.");
  cf_journal_swap();
  cc->journal = oldj;
  if (new_pool)
  {
    mp_delete(cc->pool);
    cc->pool = cc->pools ? cc->pools->pool : NULL;
  }
}

void
cf_journal_delete(void)
{
  struct cf_context *cc = cf_get_context();
  for (struct old_pools *p=cc->pools; p; p=cc->pools)
  {
    cc->pools = p->prev;
    mp_delete(p->pool);
  }
}

/* TODO: more space efficient journal */
