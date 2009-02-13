/*
 *	UCW Library -- Logging to Files
 *
 *	(c) 1997--2009 Martin Mares <mj@ucw.cz>
 *	(c) 2008 Tomas Gavenciak <gavento@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/log.h"
#include "ucw/lfs.h"
#include "ucw/threads.h"
#include "ucw/simple-lists.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

/*
 *  Use of the private fields of struct log_stream:
 *
 *	idata	file descriptor
 *	udata	various flags (FF_xxx)
 *	pdata	original name with strftime escapes
 *	name	current name of the log file
 *		(a dynamically allocated buffer)
 */

enum log_file_flag {
  FF_FORMAT_NAME = 1,		// Name contains strftime escapes
  FF_CLOSE_FD = 2,		// Close the fd with the stream
};

#define MAX_EXPAND 64		// Maximum size of expansion of strftime escapes

static int log_switch_nest;

static void
do_log_reopen(struct log_stream *ls, const char *name)
{
  int fd = ucw_open(name, O_WRONLY | O_CREAT | O_APPEND, 0666);
  if (fd < 0)
    die("Unable to open log file %s: %m", name);
  if (ls->idata < 0)
    ls->idata = fd;
  else
    {
      dup2(fd, ls->idata);
      close(fd);
    }
  if (ls->name)
    {
      xfree(ls->name);
      ls->name = NULL;		// We have to keep this consistent, die() below can invoke logging
    }
  ls->name = xstrdup(name);
}

static int
do_log_switch(struct log_stream *ls, struct tm *tm)
{
  if (!(ls->udata & FF_FORMAT_NAME))
    {
      if (ls->idata >= 0)
	return 1;
      else
	{
	  do_log_reopen(ls, ls->pdata);
	  return 1;
	}
    }

  int buflen = strlen(ls->pdata) + MAX_EXPAND;
  char name[buflen];
  int switched = 0;

  ucwlib_lock();
  if (!log_switch_nest)		// Avoid infinite loops if we die when switching logs
    {
      log_switch_nest++;
      int l = strftime(name, buflen, ls->pdata, tm);
      if (l < 0 || l >= buflen)
	die("Error formatting log file name: %m");
      if (!ls->name || strcmp(name, ls->name))
	{
	  do_log_reopen(ls, name);
	  switched = 1;
	}
      log_switch_nest--;
    }
  ucwlib_unlock();
  return switched;
}

/* Emulate the old single-file interface: close the existing log file and open a new one. */
void
log_file(const char *name)
{
  if (!name)
    return;

  struct log_stream *ls = log_new_file(name);
  struct log_stream *def = log_stream_by_flags(0);
  simp_node *s;
  while (s = clist_head(&def->substreams))
    {
      struct log_stream *old = s->p;
      log_rm_substream(def, old);
      if (old != (struct log_stream *) &log_stream_default)
	log_close_stream(old);
    }
  dup2(ls->idata, 2);			// Let fd2 be an alias for the log file
  log_add_substream(def, ls);
}

int
log_switch(void)
{
#if 0 // FIXME
  time_t tim = time(NULL);
  return do_log_switch(localtime(&tim));
#else
  return 0;
#endif
}

void
log_switch_disable(void)
{
  log_switch_nest++;
}

void
log_switch_enable(void)
{
  ASSERT(log_switch_nest);
  log_switch_nest--;
}

/* destructor for standard files */
static void
file_close(struct log_stream *ls)
{
  if ((ls->udata & FF_CLOSE_FD) && ls->idata >= 0)
    close(ls->idata);
  xfree(ls->name);
}

/* handler for standard files */
static int
file_handler(struct log_stream *ls, const char *m, uns cat UNUSED)
{
  if (ls->udata & FF_FORMAT_NAME)
    {
      // FIXME: pass the time
      time_t now = time(NULL);
      struct tm *tm = localtime(&now);
      do_log_switch(ls, tm);
    }

  int len = strlen(m);
  int r = write(ls->idata, m, len);
  /* FIXME: check for errors here? */
  return 0;
}

/* assign log to a file descriptor */
/* initialize with the default formatting, does NOT close the descriptor */
struct log_stream *
log_new_fd(int fd)
{
  struct log_stream *ls = log_new_stream();
  ls->idata = fd;
  ls->msgfmt = LSFMT_DEFAULT;
  ls->handler = file_handler;
  ls->close = file_close;
  ls->name = xmalloc(16);
  snprintf(ls->name, 16, "fd%d", fd);
  return ls;
}

/* open() a file (append mode) */
/* initialize with the default formatting */
struct log_stream *
log_new_file(const char *path)
{
  struct log_stream *ls = log_new_stream();
  ls->idata = -1;
  ls->pdata = (void *) path;
  if (strchr(path, '%'))
    ls->udata = FF_FORMAT_NAME;
  ls->udata |= FF_CLOSE_FD;
  ls->msgfmt = LSFMT_DEFAULT;
  ls->handler = file_handler;
  ls->close = file_close;

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  do_log_switch(ls, tm);		// die()'s on errors
  return ls;
}

#ifdef TEST

int main(int argc, char **argv)
{
  log_init(argv[0]);
  log_file("/proc/self/fd/1");
  // struct log_stream *ls = log_new_fd(1);
  // struct log_stream *ls = log_new_file("/tmp/quork-%Y%m%d-%H%M%S");
  for (int i=1; i<argc; i++)
    msg(L_INFO, argv[i]);
  return 0;
}

#endif
