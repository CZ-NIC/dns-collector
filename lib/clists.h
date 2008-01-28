/*
 *	UCW Library -- Circular Linked Lists
 *
 *	(c) 2003--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_CLISTS_H
#define _UCW_CLISTS_H

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

static inline int clist_empty(clist *l)
{
  return (l->head.next == &l->head);
}

#define CLIST_WALK(n,list) for(n=(void*)(list).head.next; (cnode*)(n) != &(list).head; n=(void*)((cnode*)(n))->next)
#define CLIST_WALK_DELSAFE(n,list,tmp) for(n=(void*)(list).head.next; tmp=(void*)((cnode*)(n))->next, (cnode*)(n) != &(list).head; n=(void*)tmp)
#define CLIST_FOR_EACH(type,n,list) for(type n=(void*)(list).head.next; (cnode*)(n) != &(list).head; n=(void*)((cnode*)(n))->next)
#define CLIST_FOR_EACH_DELSAFE(type,n,list,tmp) for(type n=(void*)(list).head.next; tmp=(void*)((cnode*)(n))->next, (cnode*)(n) != &(list).head; n=(void*)tmp)

#define CLIST_FOR_EACH_BACKWARDS(type,n,list) for(type n=(void*)(list).head.prev; (cnode*)(n) != &(list).head; n=(void*)((cnode*)(n))->prev)

static inline void clist_insert_after(cnode *what, cnode *after)
{
  cnode *before = after->next;
  what->next = before;
  what->prev = after;
  before->prev = what;
  after->next = what;
}

static inline void clist_insert_before(cnode *what, cnode *before)
{
  cnode *after = before->prev;
  what->next = before;
  what->prev = after;
  before->prev = what;
  after->next = what;
}

static inline void clist_add_tail(clist *l, cnode *n)
{
  clist_insert_before(n, &l->head);
}

static inline void clist_add_head(clist *l, cnode *n)
{
  clist_insert_after(n, &l->head);
}

static inline void clist_remove(cnode *n)
{
  cnode *before = n->prev;
  cnode *after = n->next;
  before->next = after;
  after->prev = before;
}

static inline void *clist_remove_head(clist *l)
{
  cnode *n = clist_head(l);
  if (n)
    clist_remove(n);
  return n;
}

static inline void *clist_remove_tail(clist *l)
{
  cnode *n = clist_tail(l);
  if (n)
    clist_remove(n);
  return n;
}

static inline void clist_init(clist *l)
{
  cnode *head = &l->head;
  head->next = head->prev = head;
}

static inline void clist_insert_list_after(clist *what, cnode *after)
{
  if (!clist_empty(what))
    {
      cnode *w = &what->head;
      w->prev->next = after->next;
      after->next->prev = w->prev;
      w->next->prev = after;
      after->next = w->next;
      clist_init(what);
    }
}

static inline uns clist_size(clist *l)
{
  uns i = 0;
  CLIST_FOR_EACH(cnode *, n, *l)
    i++;
  return i;
}

#endif
