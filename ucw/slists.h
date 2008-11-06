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

/**
 * Common header for list nodes.
 **/
typedef struct snode {
  struct snode *next;
} snode;

/**
 * Single-linked list.
 **/
typedef struct slist {
  struct snode head, *last;
} slist;

/**
 * Initialize a new single-linked list. Must be called before any other function.
 **/
static inline void slist_init(slist *l)
{
  l->head.next = l->last = NULL;
}

/**
 * Return the first node of @l or NULL if @l is empty.
 **/
static inline void *slist_head(slist *l)
{
  return l->head.next;
}

/**
 * Return the last node of @l or NULL if @l is empty.
 **/
static inline void *slist_tail(slist *l)
{
  return l->last;
}

/**
 * Find the next node to @n or NULL if @n is the last one.
 **/
static inline void *slist_next(snode *n)
{
  return n->next;
}

/**
 * Return a non-zero value iff @l is empty.
 **/
static inline int slist_empty(slist *l)
{
  return !l->head.next;
}

/**
 * Insert a new node in front of all other nodes.
 **/
static inline void slist_add_head(slist *l, snode *n)
{
  n->next = l->head.next;
  l->head.next = n;
  if (!l->last)
    l->last = n;
}

/**
 * Insert a new node after all other nodes.
 **/
static inline void slist_add_tail(slist *l, snode *n)
{
  if (l->last)
    l->last->next = n;
  else
    l->head.next = n;
  n->next = NULL;
  l->last = n;
}

/**
 * Insert a new node just after the node @after. To insert a new head, use @slist_add_head() instead.
 **/
static inline void slist_insert_after(slist *l, snode *what, snode *after)
{
  what->next = after->next;
  after->next = what;
  if (!what->next)
    l->last = what;
}

/**
 * Quickly remove the node next to @after. The node may not exist.
 **/
static inline void slist_remove_after(slist *l, snode *after)
{
  snode *n = after->next;
  if (n)
    {
      after->next = n->next;
      if (l->last == n)
        l->last = (after == &l->head) ? NULL : after;
    }
}

/**
 * Remove the first node in @l. The list can be empty.
 **/
static inline void slist_remove_head(slist *l)
{
  slist_remove_after(l, &l->head);
}

/* Loops */

#define SLIST_WALK(n,list) for(n=(void*)(list).head.next; (n); (n)=(void*)((snode*)(n))->next)
#define SLIST_WALK_DELSAFE(n,list,prev) for((prev)=(void*)&(list).head; (n)=(void*)((snode*)prev)->next; (prev)=(((snode*)(prev))->next==(snode*)(n) ? (void*)(n) : (void*)(prev)))
#define SLIST_FOR_EACH(type,n,list) for(type n=(void*)(list).head.next; n; n=(void*)((snode*)(n))->next)

/* Non-trivial functions */

/**
 * Find the previous node to @n or NULL if @n is the first one. Beware linear time complexity.
 **/
void *slist_prev(slist *l, snode *n);

/**
 * Insert a new node just before the node @before. To insert a new tail, use @slist_add_tail(). Beware linear time complexity.
 **/
void slist_insert_before(slist *l, snode *what, snode *before);

/**
 * Remove node @n. Beware linear time complexity.
 **/
void slist_remove(slist *l, snode *n);

/**
 * Remove the last node in @l. The list can be empty.
 **/
static inline void slist_remove_tail(slist *l)
{
  slist_remove(l, l->last);
}

/**
 * Compute the number of nodes in @l. Beware linear time complexity.
 **/
static inline uns slist_size(slist *l)
{
  uns i = 0;
  SLIST_FOR_EACH(snode *, n, *l)
    i++;
  return i;
}

#endif
