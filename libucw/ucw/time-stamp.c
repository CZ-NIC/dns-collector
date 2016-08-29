/*
 *	UCW Library -- A Simple Millisecond Timer
 *
 *	(c) 2007--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/time.h>

#include <sys/time.h>
#include <time.h>

#ifdef CONFIG_UCW_MONOTONIC_CLOCK

timestamp_t
get_timestamp(void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    die("clock_gettime failed: %m");
  return (timestamp_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#else

timestamp_t
get_timestamp(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (timestamp_t)tv.tv_sec * 1000 + tv.tv_usec / 1000
#ifdef CONFIG_UCW_DEBUG
	+ 3141592653	// So that we catch all attempts to corelate timestamp_t with wall clock
#endif
	;
}

#endif

#ifdef TEST

#include <stdio.h>

int main(void)
{
  printf("%ju\n", (intmax_t) get_timestamp());
  return 0;
}

#endif
