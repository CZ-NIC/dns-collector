/*
 *	UCW Library -- Logging to Syslog
 *
 *	(c) 2009 Martin Mares <mj@ucw.cz>
 *	(c) 2008 Tomas Gavenciak <gavento@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/log.h"

#include <syslog.h>

struct syslog_stream {
  struct log_stream ls;
  int facility;
};

/* Destructor */
static void
syslog_close(struct log_stream *ls)
{
  if (ls->name)
    xfree(ls->name);
}

/* convert severity level to syslog constants */
static int
syslog_level(int level)
{
  static const int levels[] = {
    [L_DEBUG] =		LOG_DEBUG,
    [L_INFO] =		LOG_INFO,
    [L_INFO_R] =	LOG_INFO,
    [L_WARN] =		LOG_WARNING,
    [L_WARN_R] =	LOG_WARNING,
    [L_ERROR] =		LOG_ERR,
    [L_ERROR_R] =	LOG_ERR,
    [L_FATAL] =		LOG_CRIT,
  };
  return ((level < (int)ARRAY_SIZE(levels)) ? levels[level] : LOG_NOTICE);
}

/* simple syslog write handler */
static int
syslog_handler(struct log_stream *ls, struct log_msg *m)
{
  struct syslog_stream *ss = (struct syslog_stream *) ls;
  int prio;
  ASSERT(ls);
  ASSERT(m);

  // FIXME: Logging of PID
  prio = syslog_level(LS_GET_LEVEL(m->flags)) | ss->facility;
  if (ls->name)
    syslog(prio, "%s: %s", ls->name, m->m);
  else
    syslog(prio, "%s", m->m);
  return 0;
}

/* assign log to a syslog facility */
/* initialize with no formatting (syslog adds these inforamtion) */
/* name is optional prefix (NULL for none) */
struct log_stream *
log_new_syslog(int facility, const char *name)
{
  struct log_stream *ls = log_new_stream(sizeof(struct syslog_stream));
  struct syslog_stream *ss = (struct syslog_stream *) ls;
  if (name)
    ls->name = xstrdup(name);
  ls->msgfmt = LSFMT_NONE;
  ls->handler = syslog_handler;
  ls->close = syslog_close;
  ss->facility = facility;
  return ls;
}
