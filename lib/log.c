/*
 *	Sherlock Library -- Logging
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include "lib.h"

static byte *progname = "???";
static pid_t pid;

static void
logit(int level, byte *msg, va_list args)
{
  time_t tim;
  struct tm *tm;
  char buf[32];

  tim = time(NULL);
  tm = localtime(&tim);
  strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", tm);
  fprintf(stderr, "%s %s [%d] <%d> ", buf, progname, pid, level);
  vfprintf(stderr, msg, args);
  fputc('\n', stderr);
  fflush(stderr);
}

void
log(byte *msg, ...)
{
  int level = 2;
  va_list args;

  va_start(args, msg);
  if (msg[0] == '<' && msg[1] >= '0' && msg[1] <= '9' && msg[2] == '>')
	{
	  level = msg[1] - '0';
	  msg += 3;
	}
  logit(level, msg, args);
  va_end(args);
}

void
die(byte *msg, ...)
{
  va_list args;

  va_start(args, msg);
  logit(9, msg, args);
  va_end(args);
  exit(99);
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
initlog(byte *argv0)
{
  if (argv0)
    progname = basename(argv0);
  pid = getpid();
}

void
open_log_file(byte *name)
{
  if (name)
    {
      int fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0666);
      if (fd < 0)
	die("Unable to open log file");
      close(2);
      dup(fd);
      close(fd);
    }
}
