/*
 *	Sherlock Library -- Logging
 *
 *	(c) 1997--2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <alloca.h>

static char log_progname[32], *log_name_patt, *log_name;
char *log_title;
static int log_pid;
static int log_params;
static int log_name_size;
int log_switch_nest;

void
log_fork(void)
{
  log_pid = getpid();
}

static void
do_log_switch(struct tm *tm)
{
  int fd, l;
  char name[log_name_size];

  if (!log_name_patt ||
      log_name[0] && !log_params)
    return;
  log_switch_nest++;
  l = strftime(name, log_name_size, log_name_patt, tm);
  if (l < 0 || l >= log_name_size)
    die("Error formatting log file name: %m");
  if (strcmp(name, log_name))
    {
      strcpy(log_name, name);
      fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0666);
      if (fd < 0)
	die("Unable to open log file %s: %m", name);
      close(2);
      dup(fd);
      close(fd);
      close(1);
      dup(2);
    }
  log_switch_nest--;
}

void
log_switch(void)
{
  time_t tim = time(NULL);
  do_log_switch(localtime(&tim));
}

static inline void
internal_log_switch(struct tm *tm)
{
  if (!log_switch_nest)
    do_log_switch(tm);
}

void
vlog_msg(unsigned int cat, const char *msg, va_list args)
{
  time_t tim = time(NULL);
  struct tm *tm = localtime(&tim);
  byte *buf, *p;
  int buflen = 256;
  int l, l0, r;

  internal_log_switch(tm);
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
#ifdef DEBUG_DIE_BY_ABORT
  abort();
#else
  exit(1);
#endif
}

#ifdef DEBUG
void
assert_failed(char *assertion, char *file, int line)
{
  log(L_FATAL, "Assertion `%s' failed at %s:%d", assertion, file, line);
  abort();
}
#else
void
assert_failed(void)
{
  die("Internal error: Assertion failed.");
}
#endif

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

void
log_file(byte *name)
{
  if (name)
    {
      if (log_name_patt)
	xfree(log_name_patt);
      if (log_name)
	{
	  xfree(log_name);
	  log_name = NULL;
	}
      log_name_patt = xstrdup(name);
      log_params = !!strchr(name, '%');
      log_name_size = strlen(name) + 64;	/* 63 is an upper bound on expansion of % escapes */
      log_name = xmalloc(log_name_size);
      log_name[0] = 0;
      log_switch();
      close(0);
      open("/dev/null", O_RDWR, 0);
    }
}
