/*
 *	UCW Library -- Logging: Configuration of Log Streams
 *
 *	(c) 2009 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/log.h"
#include "ucw/conf.h"
#include "ucw/simple-lists.h"

#include <string.h>
#include <syslog.h>

struct stream_config {
  cnode n;
  char *name;
  char *file_name;
  char *syslog_facility;
  clist substreams;			// simple_list of names
  int microseconds;			// Enable logging of precise timestamps
  int syslog_pids;
  struct log_stream *ls;
  int mark;				// Used temporarily in log_config_commit()
};

static char *
stream_commit(void *ptr)
{
  struct stream_config *c = ptr;

  if (c->file_name && c->syslog_facility)
    return "Both FileName and SyslogFacility selected";
  if (c->syslog_facility && !log_syslog_facility_exists(c->syslog_facility))
    return cf_printf("SyslogFacility `%s' is not recognized", c->syslog_facility);
  if (c->syslog_facility && c->microseconds)
    return "Syslog streams do not support microsecond precision";
  return NULL;
}

static struct cf_section stream_config = {
  CF_TYPE(struct stream_config),
  CF_COMMIT(stream_commit),
  CF_ITEMS {
#define P(x) PTR_TO(struct stream_config, x)
    CF_STRING("Name", P(name)),
    CF_STRING("FileName", P(file_name)),
    CF_STRING("SyslogFacility", P(syslog_facility)),
    CF_LIST("Substream", P(substreams), &cf_string_list_config),
    CF_INT("Microseconds", P(microseconds)),
    CF_INT("SyslogPID", P(syslog_pids)),
#undef P
    CF_END
  }
};

static clist log_stream_confs;

static struct stream_config *
stream_find(const char *name)
{
  CLIST_FOR_EACH(struct stream_config *, c, log_stream_confs)
    if (!strcmp(c->name, name))
      return c;
  return NULL;
}

static char *
stream_resolve(struct stream_config *c)
{
  if (c->mark == 2)
    return NULL;
  if (c->mark == 1)
    return cf_printf("Log stream `%s' has substreams which refer to itself", c->name);

  c->mark = 1;
  char *err;
  CLIST_FOR_EACH(simp_node *, s, c->substreams)
    {
      struct stream_config *d = stream_find(s->s);
      if (!d)
	return cf_printf("Log stream `%s' refers to unknown substream `%s'", c->name, s->s);
      if (err = stream_resolve(d))
	return err;
    }
  c->mark = 2;
  return NULL;
}

static char *
log_config_commit(void *ptr UNUSED)
{
  // Verify uniqueness of names
  CLIST_FOR_EACH(struct stream_config *, c, log_stream_confs)
    if (stream_find(c->name) != c)
      return cf_printf("Log stream `%s' defined twice", c->name);

  // Check that all substreams resolve and that there are no cycles
  char *err;
  CLIST_FOR_EACH(struct stream_config *, c, log_stream_confs)
    if (err = stream_resolve(c))
      return err;

  return NULL;
}

static struct cf_section log_config = {
  CF_COMMIT(log_config_commit),
  CF_ITEMS {
    CF_LIST("Stream", &log_stream_confs, &stream_config),
    CF_END
  }
};

static void CONSTRUCTOR
log_config_init(void)
{
  cf_declare_section("Logging", &log_config, 0);
}

char *
log_check_configured(const char *name)
{
  if (stream_find(name))
    return NULL;
  else
    return cf_printf("Log stream `%s' not found", name);
}

static struct log_stream *
do_new_configured(struct stream_config *c)
{
  struct log_stream *ls;
  ASSERT(c);

  if (c->ls)
    return c->ls;

  if (c->file_name)
    ls = log_new_file(c->file_name);
  else if (c->syslog_facility)
    ls = log_new_syslog(c->syslog_facility, (c->syslog_pids ? LOG_PID : 0));
  else
    ls = log_new_stream(sizeof(*ls));

  CLIST_FOR_EACH(simp_node *, s, c->substreams)
    log_add_substream(ls, do_new_configured(stream_find(s->s)));

  if (c->microseconds)
    ls->msgfmt |= LSFMT_USEC;

  c->ls = ls;
  return ls;
}

struct log_stream *
log_new_configured(const char *name)
{
  struct stream_config *c = stream_find(name);
  if (!c)
    die("Unable to find log stream %s", name);
  if (c->ls)
    return log_ref_stream(c->ls);
  return do_new_configured(c);
}

#ifdef TEST

#include "ucw/getopt.h"

int main(int argc, char **argv)
{
  log_init(argv[0]);
  int c;
  while ((c = cf_getopt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL)) >= 0)
    die("No options here.");

  struct log_stream *ls = log_new_configured("combined");
  msg(L_INFO | ls->regnum, "Hello, universe!");

  log_close_all();
  return 0;
}

#endif
