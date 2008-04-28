/*
 *	UCW Library -- Logging
 *
 *	(c) 1997--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <alloca.h>

static char log_progname[32];
char *log_filename;
char *log_title;
int log_pid;
int log_precise_timings;
void (*log_die_hook)(void);
void (*log_switch_hook)(struct tm *tm);

void
vmsg(unsigned int cat, const char *fmt, va_list args)
{
  struct timeval tv;
  struct tm tm;
  byte *buf, *p;
  int buflen = 256;
  int l, l0, r;
  va_list args2;

  gettimeofday(&tv, NULL);
  if (!localtime_r(&tv.tv_sec, &tm))
    bzero(&tm, sizeof(tm));

  if (log_switch_hook)
    log_switch_hook(&tm);
  while (1)
    {
      p = buf = alloca(buflen);
      *p++ = cat;
      /* We cannot use strftime() here, because it's not re-entrant */
      p += sprintf(p, " %4d-%02d-%02d %02d:%02d:%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
		   tm.tm_hour, tm.tm_min, tm.tm_sec);
      if (log_precise_timings)
        p += sprintf(p, ".%06d", (int)tv.tv_usec);
      *p++ = ' ';
      if (log_title)
	{
	  if (log_pid)
	    p += sprintf(p, "[%s (%d)] ", log_title, log_pid);
	  else
	    p += sprintf(p, "[%s] ", log_title);
	}
      else
	{
	  if (log_pid)
	    p += sprintf(p, "[%d] ", log_pid);
	}
      l0 = p - buf + 1;
      r = buflen - l0;
      va_copy(args2, args);
      l = vsnprintf(p, r, fmt, args2);
      va_end(args2);
      if (l < 0)
	l = r;
      else if (l < r)
	{
	  while (*p)
	    {
	      if (*p < 0x20 && *p != '\t')
		*p = 0x7f;
	      p++;
	    }
	  *p = '\n';
	  write(2, buf, l + l0);
	  return;
	}
      buflen = l + l0 + 1;
    }
}

void
msg(unsigned int cat, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vmsg(cat, fmt, args);
  va_end(args);
}

void
safe_vmsg(unsigned int cat, const char *fmt, va_list args)
{
  byte *buf, *p;
  int buflen = 256;
  int l, l0, r;
  va_list args2;

  while (1)
    {
      p = buf = alloca(buflen);
      *p++ = cat;
      p += sprintf(p, " \?\?\?\?-\?\?-\?\? \?\?:\?\?:\?\?");
      if (log_precise_timings)
        p += sprintf(p, ".\?\?\?\?\?\?");
      *p++ = ' ';
      if (log_title)
	{
	  if (log_pid)
	    p += sprintf(p, "[%s (%d)] ", log_title, log_pid);
	  else
	    p += sprintf(p, "[%s] ", log_title);
	}
      else
	{
	  if (log_pid)
	    p += sprintf(p, "[%d] ", log_pid);
	}
      l0 = p - buf + 1;
      r = buflen - l0;
      va_copy(args2, args);
      l = vsnprintf(p, r, fmt, args2);
      va_end(args2);
      if (l < 0)
	l = r;
      else if (l < r)
	{
	  while (*p)
	    {
	      if (*p < 0x20 && *p != '\t')
		*p = 0x7f;
	      p++;
	    }
	  *p = '\n';
	  write(2, buf, l + l0);
	  return;
	}
      buflen = l + l0 + 1;
    }
}

void
safe_msg(unsigned int cat, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  safe_vmsg(cat, fmt, args);
  va_end(args);
}

void
die(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vmsg(L_FATAL, fmt, args);
  va_end(args);
  if (log_die_hook)
    log_die_hook();
#ifdef DEBUG_DIE_BY_ABORT
  abort();
#else
  exit(1);
#endif
}

void
assert_failed(const char *assertion, const char *file, int line)
{
  msg(L_FATAL, "Assertion `%s' failed at %s:%d", assertion, file, line);
  abort();
}

void
assert_failed_noinfo(void)
{
  die("Internal error: Assertion failed.");
}

static const char *
log_basename(const char *n)
{
  const char *p = n;

  while (*n)
    if (*n++ == '/')
      p = n;
  return p;
}

void
log_init(const char *argv0)
{
  if (argv0)
    {
      strncpy(log_progname, log_basename(argv0), sizeof(log_progname)-1);
      log_progname[sizeof(log_progname)-1] = 0;
      log_title = log_progname;
    }
}
