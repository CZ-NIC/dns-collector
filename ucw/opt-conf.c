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

void opt_conf_internal(struct opt_item * opt, const char * value, void * data UNUSED) {
  struct cf_context *cc = cf_get_context();
  switch (opt->letter) {
    case 'S':
      cf_load_default(cc);
      if (cf_set(value))
	opt_failure("Cannot set %s", value);
      break;
    case 'C':
      if (cf_load(value))
	opt_failure("Cannot load config file %s", value);
      break;
#ifdef CONFIG_UCW_DEBUG
    case '0':
      opt_conf_end_of_options(cc);
      struct fastbuf *b = bfdopen(1, 4096);
      cf_dump_sections(b);
      bclose(b);
      exit(0);
      break;
#endif
  }
}

void opt_conf_hook_internal(struct opt_item * opt, const char * value UNUSED, void * data UNUSED) {
  static enum {
    OPT_CONF_HOOK_BEGIN,
    OPT_CONF_HOOK_CONFIG,
    OPT_CONF_HOOK_OTHERS
  } state = OPT_CONF_HOOK_BEGIN;

  int confopt = 0;

  if (opt->letter == 'S' || opt->letter == 'C' || (opt->name && !strcmp(opt->name, "dumpconfig")))
    confopt = 1;

  switch (state) {
    case OPT_CONF_HOOK_BEGIN:
      if (confopt)
	state = OPT_CONF_HOOK_CONFIG;
      else {
	opt_conf_end_of_options(cf_get_context());
	state = OPT_CONF_HOOK_OTHERS;
      }
      break;
    case OPT_CONF_HOOK_CONFIG:
      if (!confopt) {
	opt_conf_end_of_options(cf_get_context());
	state = OPT_CONF_HOOK_OTHERS;
      }
      break;
    case OPT_CONF_HOOK_OTHERS:
      if (confopt)
	opt_failure("Config options (-C, -S) must stand before other options.");
      break;
    default:
      ASSERT(0);
  }
}
