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
#include "ucw/log-internal.h"
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

static int default_log_handler(struct log_stream *ls UNUSED, struct log_msg *m)
{
  // This is a completely bare version of the log-file module. Errors are ignored.
  write(2, m->m, m->m_len);
  return 0;
}

struct log_stream log_stream_default = {
  .name = "stderr",
  .use_count = 1000000,
  .handler = default_log_handler,
  .levels = ~0U,
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
 *
 *  If the log-stream machinery has not been initialized (which is normal for programs
 *  with no fancy logging), the log_streams gbuf is empty and this function only
 *  translates stream #0 to the static log_stream_default.
 */

struct log_stream *
log_stream_by_flags(uns flags)
{
  int n = LS_GET_STRNUM(flags);
  if (n < 0 || n >= log_streams_after || log_streams.ptr[n]->regnum == -1)
    return (n ? NULL : &log_stream_default);
  return log_streams.ptr[n];
}

/*** Logging ***/

void
vmsg(uns cat, const char *fmt, va_list args)
{
  struct timeval tv;
  struct tm tm;
  va_list args2;
  char stime[24];
  char sutime[12];
  char msgbuf[256];
  char *p;
  int len;
  struct log_stream *ls = log_stream_by_flags(cat);
  struct log_msg m = { .flags = cat };

  /* Check the stream existence */
  if (!ls)
    {
      msg((LS_CTRL_MASK&cat)|L_WARN, "No log_stream with number %d! Logging to the default log.", LS_GET_STRNUM(cat));
      ls = &log_stream_default;
    }

  /* Get the current time */
  if (!(cat & L_SIGHANDLER))
    {
      /* CAVEAT: These calls are not safe in signal handlers. */
      gettimeofday(&tv, NULL);
      if (localtime_r(&tv.tv_sec, &tm))
	m.tm = &tm;
    }

  /* Generate time strings */
  if (m.tm)
    {
      strftime(stime, sizeof(stime), "%Y-%m-%d %H:%M:%S", &tm);
      snprintf(sutime, sizeof(sutime), ".%06d", (int)tv.tv_usec);
      m.stime = stime;
      m.sutime = sutime;
    }
  else
    {
      m.stime = "\?\?\?\?-\?\?-\?\? \?\?:\?\?:\?\?";
      m.sutime = ".\?\?\?\?\?\?";
    }

  /* Generate the message string */
  va_copy(args2, args);
  len = vsnprintf(msgbuf, sizeof(msgbuf), fmt, args2);
  va_end(args2);
  if (len < (int) sizeof(msgbuf))
    m.raw_msg = msgbuf;
  else
    {
      m.raw_msg = xmalloc(len+1);
      vsnprintf(m.raw_msg, len+1, fmt, args);
    }

  /* Remove non-printable characters and newlines */
  p = m.raw_msg;
  while (*p)
    {
      if (*p < 0x20 && *p != '\t')
	*p = 0x7f;
      p++;
    }

  /* Pass the message to the log_stream */
  if (log_pass_msg(0, ls, &m))
    {
      /* Error (such as infinite loop) occurred */
      log_pass_msg(0, &log_stream_default, &m);
    }

  if (m.raw_msg != msgbuf)
    xfree(m.raw_msg);
}

/* Maximal depth of log_pass_msg recursion */
#define LS_MAX_DEPTH 64

int
log_pass_msg(int depth, struct log_stream *ls, struct log_msg *m)
{
  ASSERT(ls);

  /* Check recursion depth */
  if (depth > LS_MAX_DEPTH)
    {
      struct log_msg errm = *m;
      errm.flags = L_ERROR | (m->flags & LS_CTRL_MASK);
      errm.raw_msg = "Loop in the log_stream system detected.";
      log_pass_msg(0, &log_stream_default, &errm);
    }

  /* Filter by level and hook function */
  if (!((1 << LS_GET_LEVEL(m->flags)) & ls->levels))
    return 0;
  if (ls->filter && ls->filter(ls, m))
    return 0;

  /* Pass the message to substreams */
  CLIST_FOR_EACH(simp_node *, s, ls->substreams)
    if (log_pass_msg(depth+1, s->p, m))
      return 1;

  /* Will pass to the handler of this stream... is there any? */
  if (!ls->handler)
    return 0;

  /* Upper bound on message length */
  int len = strlen(m->raw_msg) + strlen(m->stime) + strlen(m->sutime) + 32;
  if (log_title)
    len += strlen(log_title);
  if (ls->name)
    len += strlen(ls->name);

  /* Get a buffer and format the message */
  char *free_buf = NULL;
  if (len <= 256)
    m->m = alloca(len);
  else
    m->m = free_buf = xmalloc(len);
  char *p = m->m;

  /* Level (2 chars) */
  if (ls->msgfmt & LSFMT_LEVEL)
    {
      *p++ = LS_LEVEL_LETTER(LS_GET_LEVEL(m->flags));
      *p++ = ' ';
    }

  /* Time (|stime| + |sutime| + 1 chars) */
  if (ls->msgfmt & LSFMT_TIME)
    {
      const char *q = m->stime;
      while (*q)
	*p++ = *q++;
      if (ls->msgfmt & LSFMT_USEC)
	{
	  q = m->sutime;
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
      const char *q = m->raw_msg;
      while (*q)
	*p++ = *q++;
      *p++ = '\n';
      *p = '\0';
      m->m_len = p - m->m;
      ls->handler(ls, m);
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

void
log_fork(void)
{
  log_pid = getpid();
}

#ifdef TEST

#include <syslog.h>

int main(void)
{
  struct log_stream *ls = log_new_syslog(LOG_USER, "syslog");
  msg(L_INFO | ls->regnum, "Brum <%300s>", ":-)");
  log_set_format(log_default_stream(), ~0U, LSFMT_USEC);
  msg(L_INFO, "Brum <%300s>", ":-)");
  return 0;
}

#endif
