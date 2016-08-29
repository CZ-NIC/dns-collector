/*
 *	UCW Library -- Interface between command-line options and configuration
 *
 *	(c) 2013 Jan Moskyto Matejka <mq@ucw.cz>
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/opt.h>
#include <ucw/opt-internal.h>
#include <ucw/conf.h>
#include <ucw/conf-internal.h>
#include <ucw/fastbuf.h>

#include <alloca.h>
#include <math.h>

static void opt_conf_end_of_options(struct cf_context *cc) {
  cf_load_default(cc);
  if (cc->postpone_commit && cf_close_group())
    opt_failure("Loading of configuration failed");
}

static void opt_conf_check(struct opt_context *oc)
{
  switch (oc->conf_state) {
    case OPT_CONF_HOOK_BEGIN:
      oc->conf_state = OPT_CONF_HOOK_CONFIG;
      break;
    case OPT_CONF_HOOK_CONFIG:
      break;
    case OPT_CONF_HOOK_OTHERS:
      opt_failure("Config options must stand before other options.");
      break;
    default:
      ASSERT(0);
  }
}

void opt_handle_config(const struct opt_item * opt UNUSED, const char * value, void * data)
{
  opt_conf_check(data);
  if (cf_load(value))
    exit(1);		// Error message is already printed by cf_load()
}

void opt_handle_set(const struct opt_item * opt UNUSED, const char * value, void * data)
{
  opt_conf_check(data);
  struct cf_context *cc = cf_get_context();
  cf_load_default(cc);
  if (cf_set(value))
    opt_failure("Cannot set %s", value);
}

void opt_handle_dumpconfig(const struct opt_item * opt UNUSED, const char * value UNUSED, void * data UNUSED)
{
  struct cf_context *cc = cf_get_context();
  opt_conf_end_of_options(cc);
  struct fastbuf *b = bfdopen(1, 4096);
  cf_dump_sections(b);
  bclose(b);
  exit(0);
}

void opt_conf_hook_internal(const struct opt_item * opt, uint event, const char * value UNUSED, void * data) {
  struct opt_context *oc = data;
  struct cf_context *cc = cf_get_context();

  if (event == OPT_HOOK_FINAL) {
      opt_conf_end_of_options(cc);
      return;
  }

  ASSERT(event == OPT_HOOK_BEFORE_VALUE);

  if (opt->flags & OPT_BEFORE_CONFIG)
    return;

  switch (oc->conf_state) {
    case OPT_CONF_HOOK_BEGIN:
    case OPT_CONF_HOOK_CONFIG:
      opt_conf_end_of_options(cc);
      oc->conf_state = OPT_CONF_HOOK_OTHERS;
      break;
    case OPT_CONF_HOOK_OTHERS:
      break;
    default:
      ASSERT(0);
  }
}
