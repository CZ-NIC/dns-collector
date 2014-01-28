/*
 *	UCW Library -- Generic allocators
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 */

#ifndef _UCW_ALLOC_H
#define _UCW_ALLOC_H

struct ucw_allocator {
  void * (*alloc)(struct ucw_allocator *alloc, size_t size);
  void * (*realloc)(struct ucw_allocator *alloc, void *ptr, size_t old_size, size_t new_size);
  void (*free)(struct ucw_allocator *alloc, void *ptr);
};

/* alloc-std.c */

extern struct ucw_allocator ucw_allocator_std;
extern struct ucw_allocator ucw_allocator_zeroed;

#endif
