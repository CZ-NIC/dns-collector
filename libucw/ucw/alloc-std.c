/*
 *	UCW Library -- Generic Allocator Using Malloc
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/alloc.h>

#include <string.h>

/* Default allocator */

static void *ucw_std_alloc(struct ucw_allocator *a UNUSED, size_t size)
{
  return xmalloc(size);
}

static void *ucw_std_realloc(struct ucw_allocator *a UNUSED, void *ptr, size_t old_size UNUSED, size_t new_size)
{
  return xrealloc(ptr, new_size);
}

static void ucw_std_free(struct ucw_allocator *a UNUSED, void *ptr)
{
  xfree(ptr);
}

struct ucw_allocator ucw_allocator_std = {
  .alloc = ucw_std_alloc,
  .realloc = ucw_std_realloc,
  .free = ucw_std_free,
};

/* Zeroing allocator */

static void *ucw_zeroed_alloc(struct ucw_allocator *a UNUSED, size_t size)
{
  return xmalloc_zero(size);
}

static void *ucw_zeroed_realloc(struct ucw_allocator *a UNUSED, void *ptr, size_t old_size, size_t new_size)
{
  ptr = xrealloc(ptr, new_size);
  if (old_size < new_size)
    bzero((byte *) ptr + old_size, new_size - old_size);
  return ptr;
}

struct ucw_allocator ucw_allocator_zeroed = {
  .alloc = ucw_zeroed_alloc,
  .realloc = ucw_zeroed_realloc,
  .free = ucw_std_free,
};
