/*
 *	Sherlock Library -- Logging
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

static char *log_progname;
static pid_t log_pid;

void
log_fork(void)
{
  log_pid = getpid();
}

static void
vlog(unsigned int cat, const char *msg, va_list args)
{
  time_t tim = time(NULL);
  struct tm *tm = localtime(&tim);
  char *prog = log_progname ?: "?";
  char buf[32];

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

static byte *
basename(byte *n)
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
    log_progname = basename(argv0);
}

void
log_file(byte *name)
{
  if (name)
    {
      int fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0666);
      if (fd < 0)
	die("Unable to open log file %s: %m", name);
      close(2);
      dup(fd);
      close(fd);
    }
}
