/*
 *	Sherlock Library -- Linked Lists
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

struct node {
  struct node *next, *prev;
};
typedef struct node node;

struct list {
  struct node head, tail;
};
typedef struct list list;

#define NODE (node *)
#define HEAD(list) ((void *)((list).head.next))
#define TAIL(list) ((void *)((list).tail.prev))
#define DO_FOR_ALL(n,list) for((n)=HEAD(list);(NODE (n))->next; \
				 n=(void *)((NODE (n))->next))
#define EMPTY_LIST(list) (!(list).head.next->next)

void add_tail(list *, node *);
void add_head(list *, node *);
void rem_node(node *);
void add_tail_list(list *, list *);
void init_list(list *);
void insert_node(node *, node *);

#ifdef __GNUC__

extern inline void
add_tail(list *l, node *n)
{
  node *z = l->tail.prev;

  n->next = &l->tail;
  n->prev = z;
  z->next = n;
  l->tail.prev = n;
}

extern inline void
add_head(list *l, node *n)
{
  node *z = l->head.next;

  n->next = z;
  n->prev = &l->head;
  z->prev = n;
  l->head.next = n;
}

extern inline void
insert_node(node *n, node *after)
{
  node *z = after->next;

  n->next = z;
  n->prev = after;
  after->next = n;
  z->prev = n;
}

extern inline void
rem_node(node *n)
{
  node *z = n->prev;
  node *x = n->next;

  z->next = x;
  x->prev = z;
}

extern inline void
init_list(list *l)
{
  l->head.next = &l->tail;
  l->head.prev = NULL;
  l->tail.next = NULL;
  l->tail.prev = &l->head;
}

extern inline void
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

#endif
