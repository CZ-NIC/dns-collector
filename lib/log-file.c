/*
 *	UCW Library -- Keeping of Log Files
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
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

static char *log_name_patt;
static int log_params;
static int log_filename_size;
int log_switch_nest;

static void
do_log_switch(struct tm *tm)
{
  int fd, l;
  char name[log_filename_size];

  if (!log_name_patt ||
      log_filename[0] && !log_params)
    return;
  log_switch_nest++;
  l = strftime(name, log_filename_size, log_name_patt, tm);
  if (l < 0 || l >= log_filename_size)
    die("Error formatting log file name: %m");
  if (strcmp(name, log_filename))
    {
      strcpy(log_filename, name);
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

static void
internal_log_switch(struct tm *tm)
{
  if (!log_switch_nest)
    do_log_switch(tm);
}

void
log_file(byte *name)
{
  if (name)
    {
      if (log_name_patt)
	xfree(log_name_patt);
      if (log_filename)
	{
	  xfree(log_filename);
	  log_filename = NULL;
	}
      log_name_patt = xstrdup(name);
      log_params = !!strchr(name, '%');
      log_filename_size = strlen(name) + 64;	/* 63 is an upper bound on expansion of % escapes */
      log_filename = xmalloc(log_filename_size);
      log_filename[0] = 0;
      log_switch();
      log_switch_hook = internal_log_switch;
      close(0);
      open("/dev/null", O_RDWR, 0);
    }
}

void
log_fork(void)
{
  log_pid = getpid();
}

#ifdef TEST

int main(int argc, char **argv)
{
  log_init(argv[0]);
  log_file("/proc/self/fd/1");
  for (int i=1; i<argc; i++)
    log(L_INFO, argv[i]);
  return 0;
}

#endif
