/*
 *	Sherlock Library -- Universal Heap Macros
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define HEAP_BUBBLE_DOWN_J(heap,num,less,swap)						\
  for (;;)										\
    {											\
      l = 2*j;										\
      if (l > num)									\
	break;										\
      if (less(heap[j],heap[l]) && (l == num || less(heap[j],heap[l+1])))		\
	break;										\
      if (l != num && less(heap[l+1],heap[l]))						\
	l++;										\
      swap(heap,j,l,x);									\
      j = l;										\
    }

#define HEAP_BUBBLE_UP_J(heap,num,less,swap)						\
  while (j > 1)										\
    {											\
      u = j/2;										\
      if (less(heap[u], heap[j]))							\
	break;										\
      swap(heap,u,j,x);									\
      j = u;										\
    }

#define HEAP_INIT(type,heap,num,less,swap)						\
  do {											\
    uns i = num;									\
    uns j, l;										\
    type x;										\
    while (i >= 1)									\
      {											\
	j = i;										\
        HEAP_BUBBLE_DOWN_J(heap,num,less,swap)						\
	i--;										\
      }											\
  } while(0)

#define HEAP_DELMIN(type,heap,num,less,swap)						\
  do {											\
    uns j, l;										\
    type x;										\
    swap(heap,1,num,x);									\
    num--; 										\
    j = 1;										\
    HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)

#define HEAP_INSERT(type,heap,num,less,swap)						\
  do {											\
    uns j, u;										\
    type x;										\
    j = num;										\
    HEAP_BUBBLE_UP_J(heap,num,less,swap);						\
  } while(0)

#define HEAP_INCREASE(type,heap,num,less,swap)						\
  do {											\
    uns j, l;										\
    type x;										\
    j = 1;										\
    HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)

#define HEAP_DELETE(type,heap,num,less,swap,pos)					\
  do {											\
    uns j, l, u;									\
    type x;										\
    j = pos;										\
    swap(heap,j,num,x);									\
    num--;										\
    if (less(heap[j], heap[num+1]))							\
      HEAP_BUBBLE_UP_J(heap,num,less,swap)						\
    else										\
      HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)
