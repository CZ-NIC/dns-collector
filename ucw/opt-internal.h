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

struct opt_precomputed {
  struct opt_item * item;
  const char * name;
  short flags;
  short count;
};

void opt_precompute(struct opt_precomputed *opt, struct opt_item *item);

#endif
