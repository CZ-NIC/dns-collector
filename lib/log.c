/*
 *	Sherlock Library -- Logging
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
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

static char *log_progname, log_name_patt[64], log_name[64];
static pid_t log_pid;
static int log_params;

void
log_fork(void)
{
  log_pid = getpid();
}

static void
log_switch(struct tm *tm)
{
  int fd;
  char name[64];

  if (!log_name_patt[0] ||
      log_name[0] && !log_params)
    return;
  strftime(name, sizeof(name), log_name_patt, tm);
  if (!strcmp(name, log_name))
    return;
  strcpy(log_name, name);
  fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0666);
  if (fd < 0)
    die("Unable to open log file %s: %m", name);
  close(2);
  dup(fd);
  close(fd);
}

static void
vlog(unsigned int cat, const char *msg, va_list args)
{
  time_t tim = time(NULL);
  struct tm *tm = localtime(&tim);
  char buf[32];

  log_switch(tm);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
  fprintf(stderr, "%c %s ", cat, buf);
  if (log_progname)
    {
      if (log_pid)
	fprintf(stderr, "[%s (%d)] ", log_progname, log_pid);
      else
	fprintf(stderr, "[%s] ", log_progname);
    }
  else
    {
      if (log_pid)
	fprintf(stderr, "[%d] ", log_pid);
    }
  vfprintf(stderr, msg, args);
  fputc('\n', stderr);
  fflush(stderr);
}

void
log(unsigned int cat, const char *msg, ...)
{
  va_list args;

  va_start(args, msg);
  vlog(cat, msg, args);
  va_end(args);
}

void
die(byte *msg, ...)
{
  va_list args;

  va_start(args, msg);
  vlog(L_FATAL, msg, args);
  va_end(args);
  exit(1);
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
    log_progname = log_basename(argv0);
}

void
log_file(byte *name)
{
  if (name)
    {
      time_t tim = time(NULL);
      struct tm *tm = localtime(&tim);
      strcpy(log_name_patt, name);
      log_params = !!strchr(name, '%');
      log_name[0] = 0;
      log_switch(tm);
    }
}
