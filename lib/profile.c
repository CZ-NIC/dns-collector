/*
 *	Sherlock Library -- Poor Man's Profiler
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/profile.h"

#include <stdio.h>

#ifdef CONFIG_PROFILE_TOD
#include <sys/time.h>

void
prof_init(prof_t *c)
{
  c->sec = c->usec = 0;
}

void
prof_switch(prof_t *o, prof_t *n)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (n)
    {
      n->start_sec = tv.tv_sec;
      n->start_usec = tv.tv_usec;
    }
  if (o)
    {
      o->sec += tv.tv_sec - o->start_sec;
      o->usec += tv.tv_usec - o->start_usec;
      if (o->usec < 0)
	{
	  o->usec += 1000000;
	  o->sec++;
	}
      else while (o->usec >= 1000000)
	{
	  o->usec -= 1000000;
	  o->sec--;
	}
    }
}

int
prof_format(char *buf, prof_t *c)
{
  return sprintf(buf, "%d.%06d", c->sec, c->usec);
}
#endif

#ifdef CONFIG_PROFILE_TSC
void
prof_init(prof_t *c)
{
  c->ticks = 0;
}

int
prof_format(char *buf, prof_t *c)
{
  return sprintf(buf, "%Ld", c->ticks);
}
#endif

#ifdef CONFIG_PROFILE_KTSC
#include <fcntl.h>
#include <unistd.h>
static int self_prof_fd = -1;

void
prof_init(prof_t *c)
{
  if (self_prof_fd < 0)
    {
      self_prof_fd = open("/proc/self/profile", O_RDONLY, 0);
      if (self_prof_fd < 0)
	die("Unable to open /proc/self/profile: %m");
    }
  c->ticks_user = 0;
  c->ticks_sys = 0;
}

void
prof_switch(prof_t *o, prof_t *n)
{
  u64 u, s;
  byte buf[256];

  int l = pread(self_prof_fd, buf, sizeof(buf)-1, 0);
  ASSERT(l > 0 && l < (int)sizeof(buf)-1);
  buf[l] = 0;
  l = sscanf(buf, "%Ld%Ld", &u, &s);
  ASSERT(l == 2);

  if (n)
    {
      n->start_user = u;
      n->start_sys = s;
    }
  if (o)
    {
      u -= o->start_user;
      o->ticks_user += u;
      s -= o->start_sys;
      o->ticks_sys += s;
    }
}

int
prof_format(char *buf, prof_t *c)
{
  return sprintf(buf, "%Ld+%Ld", c->ticks_user, c->ticks_sys);
}
#endif
