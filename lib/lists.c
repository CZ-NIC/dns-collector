/*
 *	Sherlock Library -- Linked Lists
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>

#include "lists.h"

void
add_tail(list *l, node *n)
{
  node *z = l->tail.prev;

  n->next = &l->tail;
  n->prev = z;
  z->next = n;
  l->tail.prev = n;
}

void
add_head(list *l, node *n)
{
  node *z = l->head.next;

  n->next = z;
  n->prev = &l->head;
  z->prev = n;
  l->head.next = n;
}

void
insert_node(node *n, node *after)
{
  node *z = after->next;

  n->next = z;
  n->prev = after;
  after->next = n;
  z->prev = n;
}

void
rem_node(node *n)
{
  node *z = n->prev;
  node *x = n->next;

  z->next = x;
  x->prev = z;
}

void
init_list(list *l)
{
  l->head.next = &l->tail;
  l->head.prev = NULL;
  l->tail.next = NULL;
  l->tail.prev = &l->head;
}

void
add_tail_list(list *to, list *l)
{
  node *p = to->tail.prev;
  node *q = l->head.next;

  p->next = q;
  q->prev = p;
  q = l->tail.prev;
  q->next = &to->tail;
  to->tail.prev = q;
}
