/*
 *	UCW Library -- Internals of the option parser
 *
 *	(c) 2013 Jan Moskyto Matejka <mq@ucw.cz>
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_OPT_INTERNAL_H
#define _UCW_OPT_INTERNAL_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define opt_precompute ucw_opt_precompute
#endif

enum opt_conf_state {
  OPT_CONF_HOOK_BEGIN,
  OPT_CONF_HOOK_CONFIG,
  OPT_CONF_HOOK_OTHERS,
};

struct opt_context {
  const struct opt_section * options;
  struct opt_precomputed * opts;
  struct opt_precomputed ** shortopt;
  struct opt_item ** hooks;
  int opt_count;
  int hook_count;
  int positional_max;
  int positional_count;
  bool stop_parsing;
  enum opt_conf_state conf_state;
};

struct opt_precomputed {
  struct opt_item * item;
  const char * name;
  short flags;
  short count;
};

void opt_precompute(struct opt_precomputed *opt, struct opt_item *item);

#endif
