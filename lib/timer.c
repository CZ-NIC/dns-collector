/*
 *	Sherlock Library -- Execution Timing
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "lib.h"

static struct timeval last_tv;

uns
get_timer(void)
{
  struct timeval tv;
  uns diff;

  gettimeofday(&tv, NULL);
  if (tv.tv_sec < last_tv.tv_sec
      || tv.tv_sec == last_tv.tv_sec && tv.tv_usec < last_tv.tv_usec)
    diff = 0;
  else
    {
      if (tv.tv_sec == last_tv.tv_sec)
	diff = (tv.tv_usec - last_tv.tv_usec + 500) / 1000;
      else
	{
	  diff = 1000 * (tv.tv_sec - last_tv.tv_sec - 1);
	  diff += (1000500 - last_tv.tv_usec + tv.tv_usec) / 1000;
	}
    }
  last_tv = tv;
  return diff;
}

void
init_timer(void)
{
  gettimeofday(&last_tv, NULL);
}
