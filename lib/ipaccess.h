/*
 *	UCW Library -- IP address access lists
 *
 *	(c) 1997--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_IPACCESS_H
#define _UCW_IPACCESS_H

#include "lib/clists.h"

extern struct cf_section ipaccess_cf;
int ipaccess_check(clist *l, u32 ip);

/* FIXME: Hacks to make older modules compile */
struct ipaccess_list { };
#define ipaccess_init() NULL
#define ipaccess_parse(x,y,z) NULL
#define ipaccess_check_xxx(x,y) 0

#endif
