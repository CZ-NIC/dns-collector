/*
 *	UCW Library -- Universal Heap Macros
 *
 *	(c) 2001--2012 Martin Mares <mj@ucw.cz>
 *	(c) 2005--2012 Tomas Valla <tom@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/***
 * [[intro]]
 * Introduction
 * ------------
 *
 * Binary heap is a simple data structure, which for example supports efficient insertions, deletions
 * and access to the minimal inserted item. We define several macros for such operations.
 * Note that because of simplicity of heaps, we have decided to define direct macros instead
 * of a <<generic:,macro generator>> as for several other data structures in the Libucw.
 *
 * A heap is represented by a number of elements and by an array of values. Beware that we
 * index this array from one, not from zero as do the standard C arrays.
 *
 * Most macros use these parameters:
 *
 * - @type - the type of elements
 * - @num - a variable (signed or unsigned integer) with the number of elements
 * - @heap - a C array of type @type; the heap is stored in `heap[1] .. heap[num]`; `heap[0]` is unused
 * - @less - a callback to compare two element values; `less(x, y)` shall return a non-zero value iff @x is lower than @y
 * - @swap - a callback to swap two array elements; `swap(heap, i, j, t)` must swap `heap[i]` with `heap[j]` with possible help of temporary variable @t (type @type).
 *
 * A valid heap must follow these rules:
 *
 * - `num >= 0`
 * - `heap[i] >= heap[i / 2]` for each `i` in `[2, num]`
 *
 * The first element `heap[1]` is always lower or equal to all other elements.
 *
 * [[macros]]
 * Macros
 * ------
 ***/

/* For internal use. */
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

/* For internal use. */
#define HEAP_BUBBLE_UP_J(heap,num,less,swap)						\
  while (_j > 1)									\
    {											\
      _u = _j/2;									\
      if (less(heap[_u], heap[_j]))							\
	break;										\
      swap(heap,_u,_j,x);								\
      _j = _u;										\
    }

/**
 * Shuffle the items `heap[1]`, ..., `heap[num]` to get a valid heap.
 * This operation takes linear time.
 **/
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

/**
 * Delete the minimum element `heap[1]` in `O(log(n))` time. The @num variable is decremented.
 * The removed value is moved just after the resulting heap (`heap[num + 1]`).
 **/
#define HEAP_DELETE_MIN(type,heap,num,less,swap)					\
  do {											\
    uns _j, _l;										\
    type x;										\
    swap(heap,1,num,x);									\
    num--;										\
    _j = 1;										\
    HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)

/**
 * Insert a new element @elt to the heap. The @num variable is incremented.
 * This operation takes `O(log(n))` time.
 **/
#define HEAP_INSERT(type,heap,num,less,swap,elt)					\
  do {											\
    uns _j, _u;										\
    type x;										\
    heap[++num] = elt;									\
    _j = num;										\
    HEAP_BUBBLE_UP_J(heap,num,less,swap);						\
  } while(0)

/**
 * Increase `heap[pos]` to a new value @elt (greater or equal to the previous value).
 * The time complexity is `O(log(n))`.
 **/
#define HEAP_INCREASE(type,heap,num,less,swap,pos,elt)					\
  do {											\
    uns _j, _l;										\
    type x;										\
    heap[pos] = elt;									\
    _j = pos;										\
    HEAP_BUBBLE_DOWN_J(heap,num,less,swap);						\
  } while(0)

/**
 * Decrease `heap[pos]` to a new value @elt (less or equal to the previous value).
 * The time complexity is `O(log(n))`.
 **/
#define HEAP_DECREASE(type,heap,num,less,swap,pos,elt)					\
  do {											\
    uns _j, _u;										\
    type x;										\
    heap[pos] = elt;									\
    _j = pos;										\
    HEAP_BUBBLE_UP_J(heap,num,less,swap);						\
  } while(0)

/**
 * Change `heap[pos]` to a new value @elt. The time complexity is `O(log(n))`.
 * If you know that the new value is always smaller or always greater, it is faster
 * to use `HEAP_DECREASE` or `HEAP_INCREASE` respectively.
 **/
#define HEAP_REPLACE(type,heap,num,less,swap,pos,elt)					\
  do {											\
    type _elt = elt;									\
    if (less(heap[pos], _elt))								\
      HEAP_INCREASE(type,heap,num,less,swap,pos,_elt);					\
    else										\
      HEAP_DECREASE(type,heap,num,less,swap,pos,_elt);					\
  } while(0)

/**
 * Replace the minimum `heap[pos]` by a new value @elt. The time complexity is `O(log(n))`.
 **/
#define HEAP_REPLACE_MIN(type,heap,num,less,swap,elt)					\
  HEAP_INCREASE(type,heap,num,less,swap,1,elt)

/**
 * Delete an arbitrary element, given by its position. The @num variable is decremented.
 * The operation takes `O(log(n))` time.
 **/
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

/**
 * Default swapping macro.
 **/
#define HEAP_SWAP(heap,a,b,t) (t=heap[a], heap[a]=heap[b], heap[b]=t)
