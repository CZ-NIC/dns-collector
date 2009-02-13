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

static char log_progname[32];
char *log_title;
int log_pid;
void (*log_die_hook)(void);

// FIXME: duplicate?
static int ls_default_handler(struct log_stream* ls, const char *m, u32 cat UNUSED)
{
  int len = strlen(m);
  int r = write(ls->idata, m, len);
  /* TODO: check the errors here? */
  if (r!=len)
    return errno;
  return 0;
}

/* the default logger */
const struct log_stream ls_default_log={
  .name = "fd2", .idata = 2, .pdata = NULL, .regnum = 0,
  .handler = ls_default_handler,
  .levels = LS_ALL_LEVELS,
  .msgfmt = LSFMT_DEFAULT,
  // empty clist
  .substreams.head.next = (cnode *) &ls_default_log.substreams.head,
  .substreams.head.prev = (cnode *) &ls_default_log.substreams.head,
};

/*** Registry of streams and their identifiers ***/

/* The growing array of pointers to log_streams. */
struct lsbuf_t log_streams;

/* the first never-used index in ls_streams.ptr */
int log_streams_after = 0;

/* get a stream by its LS_SET_STRNUM() */
/* returns NULL for free/invalid numbers */
/* defaults to ls_default_stream when stream number 0 closed */
struct log_stream *log_stream_by_flags(uns flags)
{
  /* get the real number */
  int n = LS_GET_STRNUM(flags);
  if ((n<0) || (n>=log_streams_after) || (log_streams.ptr[n]->regnum==-1) )
  {
    if (n==0)
      return (struct log_stream *)&ls_default_log;
    else
      return NULL;
  }
  return log_streams.ptr[n];
}

/*** Logging ***/

void vmsg(unsigned int cat, const char *fmt, va_list args)
{
  struct timeval tv;
  int have_tm = 0;
  struct tm tm;
  va_list args2;
  char stime[24];
  char sutime[12];
  char *buf,*p;
  int len;
  struct log_stream *ls=log_stream_by_flags(cat);

  /* Check the stream existence */
  if(!ls)
    {
      msg((LS_INTERNAL_MASK&cat)|L_WARN, "No log_stream with number %d! Logging to the default log.",
	LS_GET_STRNUM(cat));
      ls=(struct log_stream *)&ls_default_log;
    }

  /* get the time */
  if (!(cat&LSFLAG_SIGHANDLER))
    {
      /* CAVEAT: These calls are not safe in signal handlers. */
      gettimeofday(&tv, NULL);
      if (localtime_r(&tv.tv_sec, &tm))
	have_tm = 1;
    }

  /* generate time strings */
  if (have_tm)
    {
      strftime(stime, 24, "%Y-%m-%d %H:%M:%S", &tm);
      snprintf(sutime, 12, ".%06d", (int)tv.tv_usec);
    }
  else
    {
      snprintf(stime, 24, "\?\?\?\?-\?\?-\?\? \?\?:\?\?:\?\?");
      snprintf(sutime, 12, ".\?\?\?\?\?\?");
    }

  /* generate the message string */
  va_copy(args2, args);
  /* WARN: this may be C99 specefic */
  len = vsnprintf(NULL, 0, fmt, args2);
  va_end(args2);
  buf = xmalloc(len+2);
  vsnprintf(buf, len+1, fmt, args);

  /* remove non-printable characters and newlines */
  p=buf;
  while (*p)
    {
      if (*p < 0x20 && *p != '\t')
	*p = 0x7f;
      p++;
    }

  /* pass the message to the log_stream */
  if(ls_passmsg(0, ls, stime, sutime, buf, cat))
    {
      /* error (such as looping) occured */
      ls_passmsg(0, (struct log_stream *)&ls_default_log, stime, sutime, buf, cat);
    }

  xfree(buf);
}

/* process a message (string) */
/* depth prevents undetected looping */
/* returns 1 in case of loop detection or other fatal error
 *         0 otherwise */
int ls_passmsg(int depth, struct log_stream *ls, const char *stime, const char *sutime, const char *m, u32 cat)
{
  ASSERT(ls);

  /* Check recursion depth */
  if( depth > LS_MAX_DEPTH )
    {
      ls_passmsg(0, (struct log_stream *)&ls_default_log, stime, sutime,
	"Loop in the log_stream system detected.", L_ERROR | (cat&LS_INTERNAL_MASK) );
      return 1;
    }

  /* Filter by level and filter hook */
  if(!( (1<<LS_GET_LEVEL(cat)) & ls->levels )) return 0;
  if( ls->filter )
    if( ls->filter(ls, m, cat) != 0 ) return 0;

  /* pass message to substreams */
  CLIST_FOR_EACH(simp_node *, s, ls->substreams)
    {
      if (ls_passmsg(depth+1, (struct log_stream*)(s->p), stime, sutime, m, cat))
	return 1;
    }

  /* Prepare for handler */
  if(ls->handler)
    {
      int len = strlen(m) + strlen(stime) + strlen(sutime) + 32;
      /* SHOULD be enough for all information, but beware */
      if (log_title)  len += strlen(log_title);
      if (ls->name)  len += strlen(ls->name);
      char *buf=xmalloc(len);
      char *p=buf;

      /* Level (2 chars) */
      if(ls->msgfmt & LSFMT_LEVEL)
	{
	  *p++=LS_LEVEL_LETTER(LS_GET_LEVEL(cat));
	  *p++=' ';
	}

      /* Time (|stime| + |sutime| + 1 chars) */
      if(ls->msgfmt & LSFMT_TIME)
	{
	  char *q = (char *)stime;

	  while(*q)
	    *p++=*q++;
	  if(ls->msgfmt & LSFMT_USEC)
	    {
	      q = (char *)sutime;
	      while(*q)
		*p++=*q++;
	    }
	  *p++=' ';
	}

      /* process name, PID ( |log_title| + 6 + (|PID|<=10) chars ) */
      if((ls->msgfmt & LSFMT_TITLE) && log_title)
	{
	  if(ls->msgfmt & LSFMT_PID)
	    p += sprintf(p, "[%s (%d)] ", log_title, getpid());
	  else
	    p += sprintf(p, "[%s] ", log_title);
	}
      else
	{
	  if(ls->msgfmt & LSFMT_PID)
	    p += sprintf(p, "[%d] ", getpid());
	}

      /* log_stream name ( |ls->name| + 4 chars ) */
      if(ls->msgfmt & LSFMT_LOGNAME)
	{
	  if(ls->name)
	    p += sprintf(p, "<%s> ", ls->name);
	  else
	    p += sprintf(p, "<?> ");
	}

      /* finish the string and call the handler ( |m| + 1 chars ) */
	{
	  char *q = (char *)m;

	  while(*q)
	    *p++=*q++;
	  *p++ = '\n';
	  *p++ = '\0';
	  ls->handler(ls, buf, cat);
	}
      xfree(buf);
    }
  return 0;
}

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
      strncpy(log_progname, log_basename(argv0), sizeof(log_progname)-1);
      log_progname[sizeof(log_progname)-1] = 0;
      log_title = log_progname;
    }
}

#ifdef TEST

int main(void)
{
  // ls_default_log.msgfmt |= LSFMT_USEC;
  msg(L_INFO, "Brum!");
  return 0;
}

#endif
