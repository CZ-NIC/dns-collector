/*
 *	Sherlock Library -- Circular Linked Lists
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_CLISTS_H
#define _SHERLOCK_CLISTS_H

typedef struct cnode {
  struct cnode *next, *prev;
} cnode;

typedef struct clist {
  struct cnode head;
} clist;

static inline void *clist_head(clist *l)
{
  return (l->head.next != &l->head) ? l->head.next : NULL;
}

static inline void *clist_tail(clist *l)
{
  return (l->head.prev != &l->head) ? l->head.prev : NULL;
}

static inline void *clist_next(clist *l, cnode *n)
{
  return (n->next != &l->head) ? (void *) n->next : NULL;
}

static inline void *clist_prev(clist *l, cnode *n)
{
  return (n->prev != &l->head) ? (void *) n->prev : NULL;
}

#define CLIST_WALK(n,list) for(n=(void*)(list).head.next; (cnode*)(n) != &(list).head; n=(void*)((cnode*)(n))->next)

static inline void clist_insert(cnode *what, cnode *after)
{
  cnode *before = after->next;
  what->next = before;
  what->prev = before->prev;
  before->prev = what;
  after->next = what;
}

static inline void clist_add_tail(clist *l, cnode *n)
{
  clist_insert(n, l->head.prev);
}

static inline void clist_add_head(clist *l, cnode *n)
{
  clist_insert(n, &l->head);
}

static inline void clist_remove(cnode *n)
{
  cnode *before = n->prev;
  cnode *after = n->next;
  before->next = after;
  after->prev = before;
}

static inline void clist_init(clist *l)
{
  cnode *head = &l->head;
  head->next = head->prev = head;
}

#endif
