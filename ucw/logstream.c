#include "ucw/lib.h"
#include "ucw/clists.h"
#include "ucw/simple-lists.h"

#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <alloca.h>
#include <fcntl.h>
#include "ucw/logstream.h"

/* forward declaration */
static int ls_fdfile_handler(struct log_stream* ls, const char *m, u32 cat);

/* the deafult logger */
const struct log_stream ls_default_log={
  .name = "fd2", .idata = 2, .pdata = NULL, .regnum = 0,
  .handler = ls_fdfile_handler,
  .levels = LS_ALL_LEVELS,
  .msgfmt = LSFMT_DEFAULT,
  // empty clist
  .substreams.head.next = (cnode *) &ls_default_log.substreams.head,
  .substreams.head.prev = (cnode *) &ls_default_log.substreams.head,
};

/* user de/allocated program/process name for use in the logsystem (for LSFMT_TITLE) */
char *ls_title = NULL;

/* Define an array (growing buffer) for pointers to log_streams. */
#define GBUF_TYPE struct log_stream*
#define GBUF_PREFIX(x) lsbuf_##x
#include "ucw/gbuf.h"

/* Flag indicating initialization of the module */
static int ls_initialized = 0;

/* The growing array of pointers to log_streams. */
static struct lsbuf_t ls_streams;

/* The head of the list of freed log_streams indexes in ls_streams.ptr (-1 if none free).
 * Freed positions in ls_streams.ptr are connected into a linked list in the following way:
 * ls_streams.ptr[ls_streams_free].idata is the index of next freed position (or -1) */
int ls_streams_free = -1;

/* the first never-used index in ls_streams.ptr */
int ls_streams_after = 0;

/* Initialize the logstream module.
 * It is not neccessary to call this explicitely as it is called by
 * the first ls_new()  (for backward compatibility and ease of use). */
static void ls_init_module(void)
{
  unsigned int i;
  if (ls_initialized) return;

  /* create the grow array */
  ls_streams.ptr = NULL;
  ls_streams.len = 0;
  lsbuf_set_size(&ls_streams, LS_INIT_STREAMS);

  /* bzero */
  memset(ls_streams.ptr, 0, sizeof(struct log_stream*) * (ls_streams.len));
  ls_streams_free = -1;

  ls_initialized = 1;

  /* init the default stream (0) as forwarder to fd2 */
  struct log_stream *ls = ls_new();
  ASSERT(ls == ls_streams.ptr[0]);
  ASSERT(ls->regnum == 0);
  ls->name = "default";
  ls_add_substream(ls, (struct log_stream *) &ls_default_log);

  /* log this */
  ls_msg(L_DEBUG, "logstream module initialized.");
}

/* close all open streams, un-initialize the module, free all memory,
 * use only ls_default_log */
void ls_close_all(void)
{
  int i;

  if (!ls_initialized) return;

  for (i=0; i<ls_streams_after; i++)
  {
    if (ls_streams.ptr[i]->regnum>=0)
      ls_close(ls_streams.ptr[i]);
    xfree(ls_streams.ptr[i]);
  }

  /* set to the default state */
  lsbuf_done(&ls_streams);
  ls_streams_after=0;
  ls_streams_free=-1;
  ls_initialized = 0;
}

/* add a new substream, malloc()-ate a new simp_node */
void ls_add_substream(struct log_stream *where, struct log_stream *what)
{
  ASSERT(where);
  ASSERT(what);

  simp_node *n = xmalloc(sizeof(simp_node));
  n->p = what;
  clist_add_tail(&(where->substreams), (cnode*)n);
}

/* remove all occurences of a substream, free() the simp_node */
/* return number of deleted entries */
int ls_rm_substream(struct log_stream *where, struct log_stream *what)
{
  void *tmp;
  int cnt=0;
  ASSERT(where);
  ASSERT(what);

  CLIST_FOR_EACH_DELSAFE(simp_node *, i, where->substreams, tmp)
    if (i->p == what)
    {
      clist_remove((cnode*)i);
      xfree(i);
      cnt++;
    }
  return cnt;
}

/* Return a pointer to a new stream with no handler and an empty substream list. */
struct log_stream *ls_new(void)
{
  struct log_stream *l;
  int index;

  /* initialize the array if not initialized already */
  if (unlikely(ls_initialized==0))
    ls_init_module();

  /* there is no closed stream -- allocate a new one */
  if (ls_streams_free==-1)
  {
    /* check the size of the pointer array */
    lsbuf_grow(&ls_streams, ls_streams_after+1);
    ls_streams_free = ls_streams_after++;
    ls_streams.ptr[ls_streams_free] = xmalloc(sizeof(struct log_stream));
    ls_streams.ptr[ls_streams_free]->idata = -1;
    ls_streams.ptr[ls_streams_free]->regnum = -1;
  }

  ASSERT(ls_streams_free>=0);

  /* initialize the stream */
  index = ls_streams_free;
  l = ls_streams.ptr[index];
  ls_streams_free = l->idata;
  memset(l, 0, sizeof(struct log_stream));
  l->levels = LS_ALL_LEVELS;
  l->regnum = LS_SET_STRNUM(index);
  l->substreams.head.next = &(l->substreams.head);
  l->substreams.head.prev = &(l->substreams.head);
  return l;
}

/* Close and remember given log_stream */
/* does not affect substreams, but frees the .substreams list */
void ls_close(struct log_stream *ls)
{
  void *tmp;
  ASSERT(ls);

  /* xfree() all the simp_nodes from substreams */
  CLIST_FOR_EACH_DELSAFE(simp_node *, i, ls->substreams, tmp)
  {
    clist_remove((cnode*)i);
    xfree(i);
  }

  /* close and remember the stream */
  if (ls->close!=NULL)
    ls->close(ls);
  ls->idata = ls_streams_free;
  ls_streams_free = LS_GET_STRNUM(ls->regnum);
  ls->regnum = -1;
}

/* get a stream by its LS_SET_STRNUM() */
/* returns NULL for free/invalid numbers */
/* defaults to ls_default_stream when stream number 0 closed */
struct log_stream *ls_bynum(int num)
{
  /* get the real number */
  int n = LS_GET_STRNUM(num);
  if ((n<0) || (n>=ls_streams_after) || (ls_streams.ptr[n]->regnum==-1) )
  {
    if (n==0)
      return (struct log_stream *)&ls_default_log;
    else return NULL;
  }
  return ls_streams.ptr[n];
}

/* The proposed alternative to original vmsg() */
void ls_vmsg(unsigned int cat, const char *fmt, va_list args)
{
  struct timeval tv;
  int have_tm = 0;
  struct tm tm;
  va_list args2;
  char stime[24];
  char sutime[12];
  char *buf,*p;
  int len;
  int level=LS_GET_LEVEL(cat);
  struct log_stream *ls=ls_bynum(cat);

  /* Check the stream existence */
  if(!ls)
  {
    ls_msg((LS_INTERNAL_MASK&cat)|L_WARN, "No log_stream with number %d! Logging to the default log.",
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

/* The proposed alternative to original msg() */
void ls_msg(unsigned int cat, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  ls_vmsg(cat, fmt, args);
  va_end(args);
}

/* The proposed alternative to original die() */
void ls_die(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  ls_vmsg(L_FATAL, fmt, args);
  va_end(args);
///// why this?
//  if (log_die_hook)
//    log_die_hook();
#ifdef DEBUG_DIE_BY_ABORT
  abort();
#else
  exit(1);
#endif
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
    if (ls_title)  len += strlen(ls_title);
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

    /* process name, PID ( |ls_title| + 6 + (|PID|<=10) chars ) */
    if((ls->msgfmt & LSFMT_TITLE) && ls_title)
    {
      if(ls->msgfmt & LSFMT_PID)
        p += sprintf(p, "[%s (%d)] ", ls_title, getpid());
      else
        p += sprintf(p, "[%s] ", ls_title);
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


/********** log types */


/**** standard (filedes) files */

/* destructor for standard files */
static void ls_fdfile_close(struct log_stream *ls)
{
  ASSERT(ls);
  close(ls->idata);
  if(ls->name)
    xfree(ls->name);
}

/* handler for standard files */
static int ls_fdfile_handler(struct log_stream* ls, const char *m, u32 cat)
{
  int len = strlen(m);
  int r = write(ls->idata, m, len);
  /* TODO: check the errors here? */
  if (r!=len)
    return errno;
  return 0;
}

/* assign log to a file descriptor */
/* initialize with the default formatting, does NOT close the descriptor */
struct log_stream *ls_fdfile_new(int fd)
{
  struct log_stream *ls=ls_new();
  ls->idata=fd;
  ls->msgfmt=LSFMT_DEFAULT;
  ls->handler=ls_fdfile_handler;
  return ls;
}

/* open() a file (append mode) */
/* initialize with the default formatting */
struct log_stream *ls_file_new(const char *path)
{
  struct log_stream *ls;
  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
  if (fd<0)
  {
    ls_msg(L_ERROR, "Opening logfile '%s' failed: %m.", path);
    return NULL;
  }
  ls = ls_new();
  ls->name = xstrdup(path);
  ls->idata = fd;
  ls->msgfmt = LSFMT_DEFAULT;
  ls->handler = ls_fdfile_handler;
  ls->close = ls_fdfile_close;
  return ls;
}


/**** syslog */

/* destructor for syslog logs */
static void ls_syslog_close(struct log_stream *ls)
{
  ASSERT(ls);
  if(ls->name)
    xfree(ls->name);
}

/* convert severity level to syslog constants */
static int ls_syslog_convert_level(int level)
{
  switch(level)
  {
    case L_DEBUG: return LOG_DEBUG;
    case L_INFO: return LOG_INFO;
    case L_INFO_R: return LOG_INFO;
    case L_WARN: return LOG_WARNING;
    case L_WARN_R: return LOG_WARNING;
    case L_ERROR: return LOG_ERR;
    case L_ERROR_R: return LOG_ERR;
    case L_FATAL: return LOG_CRIT;
    default: return LOG_NOTICE;
  }
}

/* simple syslog write handler */
static int ls_syslog_handler(struct log_stream *ls, const char *m, u32 flags)
{
  int prio;
  ASSERT(ls);
  ASSERT(m);

  prio = ls_syslog_convert_level(LS_GET_LEVEL(flags)) | (ls->idata);
  if (ls->name)
    syslog(prio | (ls->idata), "%s: %s", ls->name, m);
  else
    syslog(prio | (ls->idata), "%s", m);
  return 0;
}

/* assign log to a syslog facility */
/* initialize with no formatting (syslog adds these inforamtion) */
/* name is optional prefix (NULL for none) */
struct log_stream *ls_syslog_new(int facility, const char *name)
{
  struct log_stream *ls=ls_new();
  if (name) ls->name = xstrdup(name);
  ls->idata = facility;
  ls->msgfmt = LSFMT_NONE;
  ls->handler = ls_syslog_handler;
  ls->close = ls_syslog_close;
  return ls;
}
