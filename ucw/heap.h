/*
 *	UCW Library -- Universal Heap Macros
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 *	(c) 2005 Tomas Valla <tom@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define HEAP_BUBBLE_DOWN_J(heap,num,less,swap)						\
  for (;;)										\
    {											\
      _l = 2*_j;									\
      if (_l > num)									\
	break;										\
      if (less(heap[_j],heap[_l]) && (_l == num || less(heap[_j],heap[_l+1])))		\
	break;										\
      if (_l != num && less(heap[_l+1],heap[_l]))					\
	_l++;										\
      swap(heap,_j,_l,x);								\
      _j = _l;										\
    }

#define HEAP_BUBBLE_UP_J(heap,num,less,swap)						\
  while (_j > 1)									\
    {											\
      _u = _j/2;									\
      if (less(heap[_u], heap[_j]))							\
	break;										\
      swap(heap,_u,_j,x);								\
      _j = _u;										\
    }

#define HEAP_INIT(type,heap,num,less,swap)						\
  do {											\
    uns _i = num;									\
    uns _j, _l;										\
    type x;										\
    while (_i >= 1)									\
      {											\
	_j = _i;									\
        HEAP_BUBBLE_DOWN_J(heap,num,less,swap)						\
	_i--;										\
      }											\
  } while(0)

#define HEAP_DELMIN(type,heap,num,less,swap)						\
  do {											\
    uns _j, _l;										\
    type x;										\
    swap(heap,1,num,x);									\
    num--; 										\
    _j = 1;										\
    HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)

#define HEAP_INSERT(type,heap,num,less,swap)						\
  do {											\
    uns _j, _u;										\
    type x;										\
    _j = num;										\
    HEAP_BUBBLE_UP_J(heap,num,less,swap);						\
  } while(0)

#define HEAP_INCREASE(type,heap,num,less,swap,pos)					\
  do {											\
    uns _j, _l;										\
    type x;										\
    _j = pos;										\
    HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)

#define HEAP_DELETE(type,heap,num,less,swap,pos)					\
  do {											\
    uns _j, _l, _u;									\
    type x;										\
    _j = pos;										\
    swap(heap,_j,num,x);								\
    num--;										\
    if (less(heap[_j], heap[num+1]))							\
      HEAP_BUBBLE_UP_J(heap,num,less,swap)						\
    else										\
      HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)

/* Default swapping macro */
#define HEAP_SWAP(heap,a,b,t) (t=heap[a], heap[a]=heap[b], heap[b]=t)
