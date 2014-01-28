/*
 *	UCW Library -- A simple growing array of an arbitrary type
 *
 *	(c) 2010--2012 Martin Mares <mj@ucw.cz>
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/gary.h>

#include <string.h>

void *
gary_init(size_t elt_size, size_t num_elts, int zeroed)
{
  DBG("GARY: Init to %zd elements", num_elts);
  struct gary_hdr *h = xmalloc(GARY_HDR_SIZE + elt_size * num_elts);
  h->num_elts = h->have_space = num_elts;
  h->elt_size = elt_size;
  h->zeroed = zeroed;
  if (zeroed)
    bzero(GARY_BODY(h), elt_size * num_elts);
  return GARY_BODY(h);
}

static struct gary_hdr *
gary_realloc(struct gary_hdr *h, size_t n)
{
  size_t old_size = h->have_space;
  if (n > 2*h->have_space)
    h->have_space = n;
  else
    h->have_space *= 2;
  DBG("GARY: Resize from %zd to %zd elements (need %zd)", old_size, h->have_space, n);
  h = xrealloc(h, GARY_HDR_SIZE + h->have_space * h->elt_size);
  if (h->zeroed)
    bzero(GARY_BODY(h) + h->elt_size * old_size, h->elt_size * (h->have_space - old_size));
  return h;
}

void *
gary_set_size(void *array, size_t n)
{
  struct gary_hdr *h = GARY_HDR(array);
  h->num_elts = n;
  if (n <= h->have_space)
    return array;

  h = gary_realloc(h, n);
  return GARY_BODY(h);
}

void *
gary_push_helper(void *array, size_t n, byte **cptr)
{
  struct gary_hdr *h = GARY_HDR(array);
  h = gary_realloc(h, h->num_elts);
  *cptr = GARY_BODY(h) + (h->num_elts - n) * h->elt_size;
  return GARY_BODY(h);
}

void *
gary_fix(void *array)
{
  struct gary_hdr *h = GARY_HDR(array);
  if (h->num_elts != h->have_space)
    {
      h = xrealloc(h, GARY_HDR_SIZE + h->num_elts * h->elt_size);
      h->have_space = h->num_elts;
    }
  return GARY_BODY(h);
}

#ifdef TEST

#include <stdio.h>

int main(void)
{
  int *a;
  GARY_INIT_ZERO(a, 5);

  for (int i=0; i<5; i++)
    {
      ASSERT(!a[i]);
      a[i] = i+1;
    }

  GARY_PUSH(a, 1);
  *GARY_PUSH(a, 1) = 10;
  *GARY_PUSH(a, 1) = 20;
  *GARY_PUSH(a, 1) = 30;
  GARY_POP(a, 1);
  GARY_FIX(a);

  for (int i=0; i<(int)GARY_SIZE(a); i++)
    printf("%d\n", a[i]);

  GARY_FREE(a);
  return 0;
}

#endif
