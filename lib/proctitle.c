/*
 *	Sherlock Library -- Setting of Process Title
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

static char *spt_start, *spt_end;

void
setproctitle_init(int argc, char **argv)
{
#ifdef linux
#if 0					/* FIXME: This doesn't work. Why? */
  uns i, len;
  char **env, *t;

  /* Create a backup copy of environment */
  len = 0;
  for (i=0; __environ[i]; i++)
    len += strlen(__environ[i]) + 1;
  env = xmalloc(sizeof(char *)*(i+1));
  t = xmalloc(len);
  spt_end = __environ[0];
  for (i=0; __environ[i]; i++)
    {
      env[i] = t;
      len = strlen(__environ[i]) + 1;
      memcpy(t, __environ[i], len);
      t += len;
      spt_end = MAX(spt_end, __environ[i] + len);
    }
  env[i] = NULL;
  __environ[0] = NULL;
  spt_start = (byte *)(__environ+1);
  __environ = env;
  argv[0] = spt_start;
#else
  spt_start = argv[0];
  spt_end = argv[argc-1] + strlen(argv[argc-1]) - 1;
#endif
#endif
}

void
setproctitle(char *msg, ...)
{
  va_list args;
  byte buf[256];
  int n;

  va_start(args, msg);
  if (spt_end > spt_start)
    {
      n = vsnprintf(buf, sizeof(buf), msg, args);
      if (n >= (int) sizeof(buf) || n < 0)
	sprintf(buf, "<too-long>");
      n = spt_end - spt_start;
      strncpy(spt_start, buf, n);
      spt_start[n] = 0;
    }
  va_end(args);
}
