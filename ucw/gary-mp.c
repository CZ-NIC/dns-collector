/*
 *	UCW Library -- Growing arrays over mempools
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 */

#include <ucw/lib.h>
#include <ucw/gary.h>
#include <ucw/mempool.h>

#include <string.h>

static void *gary_mp_alloc(struct gary_allocator *a, size_t size)
{
  return mp_alloc(a->data, size);
}

static void *gary_mp_realloc(struct gary_allocator *a, void *ptr, size_t old_size, size_t new_size)
{
  if (new_size <= old_size)
    return ptr;

  /*
   *  In the future, we might want to do something like mp_realloc(),
   *  but we have to check that it is indeed the last block in the pool.
   */
  void *new = mp_alloc(a->data, new_size);
  memcpy(new, ptr, old_size);
  return new;
}

static void gary_mp_free(struct gary_allocator *a UNUSED, void *ptr UNUSED)
{
}

struct gary_allocator *gary_new_allocator_mp(struct mempool *mp)
{
  struct gary_allocator *a = mp_alloc(mp, sizeof(*a));
  *a = (struct gary_allocator) {
    .alloc = gary_mp_alloc,
    .realloc = gary_mp_realloc,
    .free = gary_mp_free,
    .data = mp,
  };
  return a;
}

#ifdef TEST

#include <stdio.h>

int main(void)
{
  struct mempool *mp = mp_new(4096);
  struct gary_allocator *alloc = gary_new_allocator_mp(mp);
  int *a;
  GARY_INIT_ALLOC(a, 5, alloc);

  for (int i=0; i<5; i++)
    a[i] = i+1;

  GARY_PUSH(a);
  *GARY_PUSH(a) = 10;
  *GARY_PUSH(a) = 20;
  *GARY_PUSH(a) = 30;
  GARY_POP(a);
  GARY_FIX(a);

  for (int i=0; i<(int)GARY_SIZE(a); i++)
    printf("%d\n", a[i]);

  GARY_FREE(a);
  mp_delete(mp);
  return 0;
}

#endif
