/*
 *	UCW Library -- Support routines for bitarray
 *
 *	(c) 2012 Pavel Charvat <pchar@ucw.cz>
 *	(c) 2013 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/bitops.h>
#include <ucw/bitarray.h>

uns bit_array_count_bits(bitarray_t a, uns n)
{
  uns m = 0;
  n = BIT_ARRAY_WORDS(n);
  while (n--)
    m += bit_count(*a++);
  return m;
}

bitarray_t bit_array_xrealloc(bitarray_t a, uns old_n, uns new_n)
{
  uns old_bytes = BIT_ARRAY_BYTES(old_n);
  uns new_bytes =  BIT_ARRAY_BYTES(new_n);
  if (old_bytes == new_bytes)
    return a;
  a = xrealloc(a, new_bytes);
  if (old_bytes < new_bytes)
    bzero(a + old_bytes, new_bytes - old_bytes);
  return a;
}

#ifdef TEST

#include <stdio.h>
#include <alloca.h>

int main(void)
{
  char buf[1024];
  bitarray_t a = alloca(BIT_ARRAY_BYTES(sizeof(buf)));
  while (1)
    {
      if (!fgets(buf, sizeof(buf), stdin))
	return 0;
      uns n;
      for (n = 0; buf[n] == '0' || buf[n] == '1'; n++);
      bit_array_zero(a, n);
      for (uns i = 0; i < n; i++)
	if (buf[i] == '1')
	  bit_array_set(a, i);
      printf("%u\n", bit_array_count_bits(a, n));
    }
}

#endif
