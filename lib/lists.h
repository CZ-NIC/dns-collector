/*
 *	Sherlock Library -- Linked Lists
 *
 *	(c) 1997--1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef _SHERLOCK_LISTS_H
#define _SHERLOCK_LISTS_H

typedef struct node {
  struct node *next, *prev;
} node;

typedef struct list {			/* In fact two overlayed nodes */
  struct node *head, *null, *tail;
} list;

#define NODE (node *)
#define HEAD(list) ((void *)((list).head))
#define TAIL(list) ((void *)((list).tail))
#define WALK_LIST(n,list) for(n=HEAD(list);(NODE (n))->next; \
				n=(void *)((NODE (n))->next))
#define DO_FOR_ALL(n,list) WALK_LIST(n,list)
#define WALK_LIST_DELSAFE(n,nxt,list) \
     for(n=HEAD(list); nxt=(void *)((NODE (n))->next); n=(void *) nxt)
#define WALK_LIST_BACKWARDS(n,list) for(n=TAIL(list);(NODE (n))->prev; \
				n=(void *)((NODE (n))->prev))
#define WALK_LIST_BACKWARDS_DELSAFE(n,prv,list) \
     for(n=TAIL(list); prv=(void *)((NODE (n))->prev); n=(void *) prv)

#define EMPTY_LIST(list) (!(list).head->next)

void add_tail(list *, node *);
void add_head(list *, node *);
void rem_node(node *);
void add_tail_list(list *, list *);
void init_list(list *);
void insert_node(node *, node *);

#if !defined(_SHERLOCK_LISTS_C) && defined(__GNUC__)
#define LIST_INLINE extern inline
#include "lib/lists.c"
#undef LIST_INLINE
#else
#define LIST_INLINE
#endif

#endif
