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

void
init_timer(timestamp_t *timer)
{
  *timer = get_timestamp();
}

uint
get_timer(timestamp_t *timer)
{
  timestamp_t t = *timer;
  *timer = get_timestamp();
  return MIN(*timer-t, ~0U);
}

uint
switch_timer(timestamp_t *oldt, timestamp_t *newt)
{
  *newt = get_timestamp();
  return MIN(*newt-*oldt, ~0U);
}

#ifdef TEST

#include <stdio.h>
#include <unistd.h>

int main(void)
{
  timestamp_t t;
  init_timer(&t);
  usleep(50000);
  printf("%u\n", get_timer(&t));
  return 0;
}

#endif
