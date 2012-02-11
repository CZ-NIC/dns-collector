/*
 *	UCW Library -- Memory Allocation
 *
 *	(c) 2000 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>

#include <stdlib.h>
#include <string.h>

void *
xmalloc(size_t size)
{
  void *x = malloc(size);
  if (!x)
    die("Cannot allocate %zu bytes of memory", size);
  return x;
}

void *
xmalloc_zero(size_t size)
{
  void *x = xmalloc(size);
  bzero(x, size);
  return x;
}

void
xfree(void *ptr)
{
  /*
   * Maybe it is a little waste of resources to make this a function instead
   * of a macro, but xmalloc() is not used for anything critical anyway,
   * so let's prefer simplicity.
   */
  free(ptr);
}
