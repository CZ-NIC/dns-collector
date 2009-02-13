/*
 *	UCW Library -- Logging
 *
 *	(c) 1997--2009 Martin Mares <mj@ucw.cz>
 *	(c) 2008 Tomas Gavenciak <gavento@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/log.h"
#include "ucw/simple-lists.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <alloca.h>
#include <errno.h>

char *log_title;
int log_pid;
void (*log_die_hook)(void);

/*** The default log stream, which logs to stderr ***/

static int default_log_handler(struct log_stream *ls, const char *m, uns cat UNUSED)
{
  // This is a completely bare version of the log-file module. Errors are ignored.
  write(ls->idata, m, strlen(m));
  return 0;
}

const struct log_stream log_stream_default = {
  .name = "stderr",
  .idata = 2,
  .handler = default_log_handler,
  .levels = LS_ALL_LEVELS,
  .msgfmt = LSFMT_DEFAULT,
  // an empty clist
  .substreams.head.next = (cnode *) &log_stream_default.substreams.head,
  .substreams.head.prev = (cnode *) &log_stream_default.substreams.head,
};

/*** Registry of streams and their identifiers ***/

struct lsbuf_t log_streams;		/* A growing array of pointers to log_streams */
int log_streams_after = 0;		/* The first never-used index in log_streams.ptr */

/*
 *  Find a stream by its identifier given as LS_SET_STRNUM(flags).
 *  Returns NULL if the stream doesn't exist or it's invalid.
 *  When stream #0 is requested, fall back to log_stream_default.
 */

struct log_stream *
log_stream_by_flags(uns flags)
{
  int n = LS_GET_STRNUM(flags);
  if (n < 0 || n >= log_streams_after || log_streams.ptr[n]->regnum == -1)
    return (n ? NULL : (struct log_stream *) &log_stream_default);
  return log_streams.ptr[n];
}

/*** Logging ***/

void
vmsg(uns cat, const char *fmt, va_list args)
{
  struct timeval tv;
  int have_tm = 0;
  struct tm tm;
  va_list args2;
  char stime[24];
  char sutime[12];
  char msgbuf[256];
  char *m, *p;
  int len;
  struct log_stream *ls = log_stream_by_flags(cat);

  /* Check the stream existence */
  if (!ls)
    {
      msg((LS_INTERNAL_MASK&cat)|L_WARN, "No log_stream with number %d! Logging to the default log.", LS_GET_STRNUM(cat));
      ls = (struct log_stream *) &log_stream_default;
    }

  /* Get the current time */
  if (!(cat & LSFLAG_SIGHANDLER))
    {
      /* CAVEAT: These calls are not safe in signal handlers. */
      gettimeofday(&tv, NULL);
      if (localtime_r(&tv.tv_sec, &tm))
	have_tm = 1;
    }

  /* Generate time strings */
  if (have_tm)
    {
      strftime(stime, sizeof(stime), "%Y-%m-%d %H:%M:%S", &tm);
      snprintf(sutime, sizeof(sutime), ".%06d", (int)tv.tv_usec);
    }
  else
    {
      snprintf(stime, sizeof(stime), "\?\?\?\?-\?\?-\?\? \?\?:\?\?:\?\?");
      snprintf(sutime, sizeof(sutime), ".\?\?\?\?\?\?");
    }

  /* Generate the message string */
  va_copy(args2, args);
  len = vsnprintf(msgbuf, sizeof(msgbuf), fmt, args2);
  va_end(args2);
  if (len < (int) sizeof(msgbuf))
    m = msgbuf;
  else
    {
      m = xmalloc(len+1);
      vsnprintf(m, len+1, fmt, args);
    }

  /* Remove non-printable characters and newlines */
  p = m;
  while (*p)
    {
      if (*p < 0x20 && *p != '\t')
	*p = 0x7f;
      p++;
    }

  /* Pass the message to the log_stream */
  if (log_pass_msg(0, ls, stime, sutime, m, cat))
    {
      /* Error (such as infinite loop) occurred */
      log_pass_msg(0, (struct log_stream *) &log_stream_default, stime, sutime, m, cat);
    }

  if (m != msgbuf)
    xfree(m);
}

int
log_pass_msg(int depth, struct log_stream *ls, const char *stime, const char *sutime, const char *m, uns cat)
{
  ASSERT(ls);

  /* Check recursion depth */
  if (depth > LS_MAX_DEPTH)
    {
      log_pass_msg(0, (struct log_stream *) &log_stream_default, stime, sutime,
	"Loop in the log_stream system detected.", L_ERROR | (cat & LS_INTERNAL_MASK));
      return 1;
    }

  /* Filter by level and hook function */
  if (!((1 << LS_GET_LEVEL(cat)) & ls->levels))
    return 0;
  if (ls->filter && ls->filter(ls, m, cat))
    return 0;

  /* Pass the message to substreams */
  CLIST_FOR_EACH(simp_node *, s, ls->substreams)
    if (log_pass_msg(depth+1, s->p, stime, sutime, m, cat))
      return 1;

  /* Will pass to the handler of this stream... is there any? */
  if (!ls->handler)
    return 0;

  /* Upper bound on message length */
  int len = strlen(m) + strlen(stime) + strlen(sutime) + 32;
  if (log_title)
    len += strlen(log_title);
  if (ls->name)
    len += strlen(ls->name);

  /* Get a buffer and format the message */
  char *free_buf = NULL;
  char *buf;
  if (len <= 256)
    buf = alloca(len);
  else
    buf = free_buf = xmalloc(len);
  char *p = buf;

  /* Level (2 chars) */
  if (ls->msgfmt & LSFMT_LEVEL)
    {
      *p++ = LS_LEVEL_LETTER(LS_GET_LEVEL(cat));
      *p++ = ' ';
    }

  /* Time (|stime| + |sutime| + 1 chars) */
  if (ls->msgfmt & LSFMT_TIME)
    {
      const char *q = (char *)stime;
      while (*q)
	*p++ = *q++;
      if (ls->msgfmt & LSFMT_USEC)
	{
	  q = sutime;
	  while (*q)
	    *p++ = *q++;
	}
      *p++ = ' ';
    }

  /* Process name, PID ( |log_title| + 6 + (|PID|<=10) chars ) */
  if ((ls->msgfmt & LSFMT_TITLE) && log_title)
    {
      if (ls->msgfmt & LSFMT_PID)
	p += sprintf(p, "[%s (%d)] ", log_title, log_pid);
      else
	p += sprintf(p, "[%s] ", log_title);
    }
  else
    {
      if (ls->msgfmt & LSFMT_PID)
	p += sprintf(p, "[%d] ", log_pid);
    }

  /* log_stream name ( |ls->name| + 4 chars ) */
  if (ls->msgfmt & LSFMT_LOGNAME)
    {
      if (ls->name)
	p += sprintf(p, "<%s> ", ls->name);
      else
	p += sprintf(p, "<?> ");
    }

  /* The message itself ( |m| + 1 chars ) */
    {
      const char *q = m;
      while (*q)
	*p++ = *q++;
      *p++ = '\n';
      *p++ = '\0';
      ls->handler(ls, buf, cat);
    }

  if (free_buf)
    xfree(free_buf);
  return 0;
}

/*** Utility functions ***/

void
msg(unsigned int cat, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vmsg(cat, fmt, args);
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
      static char log_progname[32];
      strncpy(log_progname, log_basename(argv0), sizeof(log_progname)-1);
      log_progname[sizeof(log_progname)-1] = 0;
      log_title = log_progname;
    }
}

#ifdef TEST

int main(void)
{
  msg(L_INFO, "Brum <%300s>", ":-)");
  return 0;
}

#endif
