/*
 *	UCW Library -- Single-Linked Lists
 *
 *	(c) 2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_SLISTS_H
#define _UCW_SLISTS_H

typedef struct snode {
  struct snode *next;
} snode;

typedef struct slist {
  struct snode head, *last;
} slist;

static inline void *slist_head(slist *l)
{
  return l->head.next;
}

static inline void *slist_tail(slist *l)
{
  return l->last;
}

static inline void *slist_next(snode *n)
{
  return n->next;
}

static inline int slist_empty(slist *l)
{
  return !l->head.next;
}

#define SLIST_WALK(n,list) for(n=(void*)(list).head.next; (n); (n)=(void*)((snode*)(n))->next)
#define SLIST_WALK_DELSAFE(n,list,prev) for((prev)=(void*)&(list).head; (n)=(void*)((snode*)prev)->next; (prev)=(((snode*)(prev))->next==(snode*)(n) ? (void*)(n) : (void*)(prev)))
#define SLIST_FOR_EACH(type,n,list) for(type n=(void*)(list).head.next; n; n=(void*)((snode*)(n))->next)

static inline void slist_insert_after(slist *l, snode *what, snode *after)
{
  what->next = after->next;
  after->next = what;
  if (!what->next)
    l->last = what;
}

static inline void slist_add_head(slist *l, snode *n)
{
  n->next = l->head.next;
  l->head.next = n;
  if (!l->last)
    l->last = n;
}

static inline void slist_add_tail(slist *l, snode *n)
{
  if (l->last)
    l->last->next = n;
  else
    l->head.next = n;
  n->next = NULL;
  l->last = n;
}

static inline void slist_init(slist *l)
{
  l->head.next = l->last = NULL;
}

static inline void slist_remove_after(slist *l, snode *after)
{
  snode *n = after->next;
  after->next = n->next;
  if (l->last == n)
    l->last = (after == &l->head) ? NULL : after;
}

/* Non-trivial functions */

void *slist_prev(slist *l, snode *n);
void slist_insert_before(slist *l, snode *what, snode *before);
void slist_remove(slist *l, snode *n);

#endif
