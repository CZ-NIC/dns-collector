/*
 *	UCW Library -- A simple growing array of an arbitrary type
 *
 *	(c) 2010--2014 Martin Mares <mj@ucw.cz>
 */

#ifndef _UCW_GARY_H
#define _UCW_GARY_H

#include <ucw/alloc.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define gary_empty_hdr ucw_gary_empty_hdr
#define gary_fix ucw_gary_fix
#define gary_init ucw_gary_init
#define gary_push_helper ucw_gary_push_helper
#define gary_set_size ucw_gary_set_size
#endif

struct gary_hdr {
  size_t num_elts;
  size_t have_space;
  size_t elt_size;
  struct ucw_allocator *allocator;
};

#define GARY_HDR_SIZE ALIGN_TO(sizeof(struct gary_hdr), CPU_STRUCT_ALIGN)
#define GARY_HDR(ptr) ((struct gary_hdr *)((byte*)(ptr) - GARY_HDR_SIZE))
#define GARY_BODY(ptr) ((byte *)(ptr) + GARY_HDR_SIZE)

/**
 * Create a new growing array, initially containing @n elements,
 * and let @ptr point to its first element. The memory used by the
 * array is allocated by <<basics:xmalloc()>>.
 **/
#define GARY_INIT(ptr, n) (ptr) = gary_init(sizeof(*(ptr)), (n), &ucw_allocator_std)

/**
 * Create a growing array like GARY_INIT() does, but all newly
 * allocated elements will be automatically zeroed.
 **/
#define GARY_INIT_ZERO(ptr, n) (ptr) = gary_init(sizeof(*(ptr)), (n), &ucw_allocator_zeroed)

/**
 * Create a growing array like GARY_INIT() does, but based upon the given
 * <<alloc:,generic allocator>>.
 **/
#define GARY_INIT_ALLOC(ptr, n, a) (ptr) = gary_init(sizeof(*(ptr)), (n), (a))

/**
 * Create a growing array, initially containing 0 elements, but with enough
 * space to keep @n of them without needing reallocation. The @ptr variable
 * will point to the first element of the array.
 **/
#define GARY_INIT_SPACE(ptr, n) do { GARY_INIT(ptr, n); (GARY_HDR(ptr))->num_elts = 0; } while (0)

/** A combination of GARY_INIT_ZERO() and GARY_INIT_SPACE(). **/
#define GARY_INIT_SPACE_ZERO(ptr, n) do { GARY_INIT_ZERO(ptr, n); (GARY_HDR(ptr))->num_elts = 0; } while (0)

/** A combination of GARY_INIT_ALLOC() and GARY_INIT_SPACE(). **/
#define GARY_INIT_SPACE_ALLOC(ptr, n, a) do { GARY_INIT_ALLOC(ptr, n, a); (GARY_HDR(ptr))->num_elts = 0; } while (0)

/** Destroy a growing array and free memory used by it. If @ptr is NULL, nothing happens. **/
#define GARY_FREE(ptr) gary_free(ptr)

/** Return the current number elements of the given growing array. **/
#define GARY_SIZE(ptr) (GARY_HDR(ptr)->num_elts)

/**
 * Resize the given growing array to @n elements.
 * The @ptr can change, if the array has to be re-allocated.
 **/
#define GARY_RESIZE(ptr, n) ((ptr) = gary_set_size((ptr), (n)))

/** Create a new growing array, or resize it if it already exists. **/
#define GARY_INIT_OR_RESIZE(ptr, n) (ptr) = (ptr) ? gary_set_size((ptr), (n)) : gary_init(sizeof(*(ptr)), (n), &ucw_allocator_std)

/**
 * Push @n elements to a growing array. That is, make space for @n more elements
 * at the end of the array and return a pointer to the first of these elements.
 * The @ptr can change, if the array has to be re-allocated.
 **/
#define GARY_PUSH_MULTI(ptr, n) ({					\
  struct gary_hdr *_h = GARY_HDR(ptr);					\
  typeof(*(ptr)) *_c = &(ptr)[_h->num_elts];				\
  size_t _n = n;							\
  _h->num_elts += _n;							\
  if (_h->num_elts > _h->have_space)					\
    (ptr) = gary_push_helper((ptr), _n, (byte **) &_c);			\
  _c; })

/**
 * Push a single element at the end of a growing array and return a pointer to it.
 * The @ptr can change, if the array has to be re-allocated.
 **/
#define GARY_PUSH(ptr) GARY_PUSH_MULTI(ptr, 1)

/**
 * Pop @n elements from the end of a growing array.
 * The @ptr can change, if the array has to be re-allocated.
 **/
#define GARY_POP_MULTI(ptr, n) GARY_HDR(ptr)->num_elts -= (n)

/**
 * Pop a single element from the end of a growing array.
 * The @ptr can change, if the array has to be re-allocated.
 **/
#define GARY_POP(ptr) GARY_POP_MULTI(ptr, 1)

/**
 * Fix size of a growing array, returning all unused memory to the
 * system (or more precisely, to the underlying allocator).
 * The @ptr can change.
 **/
#define GARY_FIX(ptr) (ptr) = gary_fix((ptr))

/* Internal functions */
void *gary_init(size_t elt_size, size_t num_elts, struct ucw_allocator *allocator);
void *gary_set_size(void *array, size_t n);
void *gary_push_helper(void *array, size_t n, byte **cptr);
void *gary_fix(void *array);

static inline void gary_free(void *ptr)
{
  if (ptr)
    {
      struct gary_hdr *h = GARY_HDR(ptr);
      h->allocator->free(h->allocator, h);
    }
}

/* A forever empty gary. Used internally. */

extern struct gary_hdr gary_empty_hdr;
#define GARY_FOREVER_EMPTY GARY_BODY(&gary_empty_hdr)

/* Type-agnostic interface. Currently it's recommended for internal use only. */

#define GARY_PUSH_GENERIC(ptr) ({					\
  struct gary_hdr *_h = GARY_HDR(ptr);					\
  void *_c = (byte *)(ptr) + _h->num_elts++ * _h->elt_size;		\
  if (_h->num_elts > _h->have_space)					\
    (ptr) = gary_push_helper((ptr), 1, (byte **) &_c);			\
  _c; })

#endif
