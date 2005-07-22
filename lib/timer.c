/*
 *	UCW Library -- Execution Timing
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

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

void
get_last_timeval(struct timeval *tv)
{
  *tv = last_tv;
}
