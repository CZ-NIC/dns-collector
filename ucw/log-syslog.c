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
static int ls_syslog_handler(struct log_stream *ls, const char *m, uns flags)
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
  struct log_stream *ls=log_new_stream();
  if (name) ls->name = xstrdup(name);
  ls->idata = facility;
  ls->msgfmt = LSFMT_NONE;
  ls->handler = ls_syslog_handler;
  ls->close = ls_syslog_close;
  return ls;
}
