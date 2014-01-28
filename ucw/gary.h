/*
 *	UCW Library -- A simple growing array of an arbitrary type
 *
 *	(c) 2010--2014 Martin Mares <mj@ucw.cz>
 */

#ifndef _UCW_GARY_H
#define _UCW_GARY_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define gary_fix ucw_gary_fix
#define gary_init ucw_gary_init
#define gary_push_helper ucw_gary_push_helper
#define gary_set_size ucw_gary_set_size
#endif

struct gary_hdr {
  size_t num_elts;
  size_t have_space;
  size_t elt_size;
  int zeroed;
};

#define GARY_HDR_SIZE ALIGN_TO(sizeof(struct gary_hdr), CPU_STRUCT_ALIGN)
#define GARY_HDR(ptr) ((struct gary_hdr *)((byte*)(ptr) - GARY_HDR_SIZE))
#define GARY_BODY(ptr) ((byte *)(ptr) + GARY_HDR_SIZE)

#define GARY_INIT(ptr, n) (ptr) = gary_init(sizeof(*(ptr)), (n), 0)
#define GARY_INIT_ZERO(ptr, n) (ptr) = gary_init(sizeof(*(ptr)), (n), 1)
#define GARY_INIT_SPACE(ptr, n) do { GARY_INIT(ptr, n); (GARY_HDR(ptr))->num_elts = 0; } while (0)
#define GARY_INIT_SPACE_ZERO(ptr, n) do { GARY_INIT_ZERO(ptr, n); (GARY_HDR(ptr))->num_elts = 0; } while (0)
#define GARY_FREE(ptr) do { if (ptr) xfree(GARY_HDR(ptr)); } while (0)
#define GARY_SIZE(ptr) (GARY_HDR(ptr)->num_elts)
#define GARY_RESIZE(ptr, n) ((ptr) = gary_set_size((ptr), (n)))
#define GARY_INIT_OR_RESIZE(ptr, n) (ptr) = (ptr) ? gary_set_size((ptr), (n)) : gary_init(sizeof(*(ptr)), (n), 0)

#define GARY_PUSH(ptr, n) ({						\
  struct gary_hdr *_h = GARY_HDR(ptr);					\
  typeof(*(ptr)) *_c = &(ptr)[_h->num_elts];				\
  size_t _n = n;							\
  _h->num_elts += _n;							\
  if (_h->num_elts > _h->have_space)					\
    (ptr) = gary_push_helper((ptr), _n, (byte **) &_c);			\
  _c; })

#define GARY_POP(ptr, n) GARY_HDR(ptr)->num_elts -= (n)
#define GARY_FIX(ptr) (ptr) = gary_fix((ptr))

/* Internal functions */
void *gary_init(size_t elt_size, size_t num_elts, int zeroed);
void *gary_set_size(void *array, size_t n);
void *gary_push_helper(void *array, size_t n, byte **cptr);
void *gary_fix(void *array);

#endif
