/*
 *	Sherlock Library -- Prime Number Tests
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>

#include "lib/lib.h"

static int				/* Sequential search */
__isprime(uns x)			/* We know x != 2 && x != 3 */
{
  uns test = 5;

  if (x == 5)
    return 1;
  for(;;)
    {
      if (!(x % test))
	return 0;
      if (x / test <= test)
	return 1;
      test += 2;			/* 6k+1 */
      if (!(x % test))
	return 0;
      if (x / test <= test)
	return 1;
      test += 4;			/* 6k-1 */
    }
}

int
isprime(uns x)
{
  if (x < 5)
    return (x == 2 || x == 3);
  switch (x % 6)
    {
    case 1:
    case 5:
      return __isprime(x);
    default:
      return 0;
    }
}

uns
nextprime(uns x)			/* Returns some prime greater than X, usually the next one or the second next one */
{
  x += 5 - (x % 6);			/* x is 6k-1 */
  for(;;)
    {
      if (__isprime(x))
	return x;
      x += 2;				/* 6k+1 */
      if (__isprime(x))
	return x;
      x += 4;				/* 6k-1 */
    }
}

#ifdef PRIME_DEBUG

int
main(int argc, char **argv)
{
  uns k = atol(argv[1]);
  if (isprime(k))
    printf("%d is prime\n");
  else
    printf("Next prime is %d\n", nextprime(k));
}

#endif
