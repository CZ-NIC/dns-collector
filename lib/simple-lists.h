/*
 *	UCW Library -- Linked Lists of Simple Items
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_SIMPLE_LISTS_H
#define _UCW_SIMPLE_LISTS_H

#include "lib/clists.h"

typedef struct simp_node {
  cnode n;
  union {
    char *s;
    void *p;
    int i;
    uns u;
  };
} simp_node;

typedef struct simp2_node {
  cnode n;
  union {
    char *s1;
    void *p1;
    int i1;
    uns u1;
  };
  union {
    char *s2;
    void *p2;
    int i2;
    uns u2;
  };
} simp2_node;

struct mempool;
simp_node *simp_append(struct mempool *mp, clist *l);
simp2_node *simp2_append(struct mempool *mp, clist *l);

/* Configuration sections */
extern struct cf_section cf_string_list_config;
extern struct cf_section cf_2string_list_config;

#endif
