/*
 *	Sherlock Library -- Logging
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <alloca.h>

static char log_progname[32];
char *log_filename;
char *log_title;
int log_pid;
void (*log_die_hook)(void);
void (*log_switch_hook)(struct tm *tm);

void
vlog_msg(unsigned int cat, const char *msg, va_list args)
{
  time_t tim = time(NULL);
  struct tm *tm = localtime(&tim);
  byte *buf, *p;
  int buflen = 256;
  int l, l0, r;

  if (log_switch_hook)
    log_switch_hook(tm);
  while (1)
    {
      p = buf = alloca(buflen);
      *p++ = cat;
      p += strftime(p, buflen, " %Y-%m-%d %H:%M:%S ", tm);
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
      l = vsnprintf(p, r, msg, args);
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
log_msg(unsigned int cat, const char *msg, ...)
{
  va_list args;

  va_start(args, msg);
  vlog_msg(cat, msg, args);
  va_end(args);
}

void
die(byte *msg, ...)
{
  va_list args;

  va_start(args, msg);
  vlog_msg(L_FATAL, msg, args);
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
assert_failed(char *assertion, char *file, int line)
{
  log(L_FATAL, "Assertion `%s' failed at %s:%d", assertion, file, line);
  abort();
}

void
assert_failed_noinfo(void)
{
  die("Internal error: Assertion failed.");
}

static byte *
log_basename(byte *n)
{
  byte *p = n;

  while (*n)
    if (*n++ == '/')
      p = n;
  return p;
}

void
log_init(byte *argv0)
{
  if (argv0)
    {
      strncpy(log_progname, log_basename(argv0), sizeof(log_progname)-1);
      log_progname[sizeof(log_progname)-1] = 0;
      log_title = log_progname;
    }
}
