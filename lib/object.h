/*
 *	Sherlock Library -- Object Functions
 *
 *	(c) 1997--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_OBJECT_H
#define _SHERLOCK_OBJECT_H

/* FIXME: Buggy objects with long attributes still exist in old databases. Decrease to 1024 one day. */
#define MAX_ATTR_SIZE 4096		/* Maximum length an attribute can ever have (including name and trailing 0) */

struct fastbuf;

struct odes {				/* Object description */
  struct oattr *attrs;
  struct mempool *pool;
  struct oattr *cached_attr;
};

struct oattr {				/* Object attribute */
  struct oattr *next, *same;
  byte attr;
  byte val[1];
};

void obj_dump(struct odes *);
struct odes *obj_new(struct mempool *);
int obj_read(struct fastbuf *, struct odes *);
void obj_write(struct fastbuf *, struct odes *);
void obj_write_nocheck(struct fastbuf *, struct odes *);
struct oattr *obj_find_attr(struct odes *, uns);
struct oattr *obj_find_attr_last(struct odes *, uns);
uns obj_del_attr(struct odes *, struct oattr *);
byte *obj_find_aval(struct odes *, uns);
struct oattr *obj_set_attr(struct odes *, uns, byte *);
struct oattr *obj_set_attr_num(struct odes *, uns, uns);
struct oattr *obj_add_attr(struct odes *, uns, byte *);
struct oattr *obj_prepend_attr(struct odes *, uns, byte *);
struct oattr *obj_insert_attr(struct odes *o, struct oattr *first, struct oattr *after, byte *v);

#endif
