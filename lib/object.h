/*
 *	Sherlock Library -- Object Functions
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_OBJECT_H
#define _SHERLOCK_OBJECT_H

#define MAX_ATTR_SIZE 2500
  /* Maximum length an attribute can ever have (including name and trailing 0).
   * It has to be long enough to contain 1 URL, 1 reftext, and a few numbers
   * (see 'x' attribute in labels).  */

struct fastbuf;
struct mempool;

struct odes {				/* Object description */
  struct oattr *attrs;
  struct mempool *pool;
  struct oattr *cached_attr;
};

struct oattr {				/* Object attribute */
  struct oattr *next, *same;
  byte attr;
  byte *val;
};

void obj_dump(struct odes *);
struct odes *obj_new(struct mempool *);
struct oattr *obj_find_attr(struct odes *, uns);
struct oattr *obj_find_attr_last(struct odes *, uns);
uns obj_del_attr(struct odes *, struct oattr *);
byte *obj_find_aval(struct odes *, uns);
uns obj_find_anum(struct odes *, uns, uns);
struct oattr *obj_set_attr(struct odes *, uns, byte *);
struct oattr *obj_set_attr_num(struct odes *, uns, uns);
struct oattr *obj_add_attr(struct odes *, uns, byte *);
struct oattr *obj_add_attr_ref(struct odes *o, uns x, byte *v);	// no strdup()
struct oattr *obj_prepend_attr(struct odes *, uns, byte *);
struct oattr *obj_insert_attr(struct odes *o, struct oattr *first, struct oattr *after, byte *v);
void obj_move_attr_to_head(struct odes *o, uns);
void obj_move_attr_to_tail(struct odes *o, uns);

/* buck2obj.c: Reading of objects from buckets */

struct parsed_attr {
  int attr;
  byte *val;
  uns len;
};
struct buck2obj_buf;

void get_attr_set_type(uns type);
int get_attr(byte **pos, byte *end, struct parsed_attr *attr);
int bget_attr(struct fastbuf *b, struct parsed_attr *attr);

struct buck2obj_buf *buck2obj_alloc(void);
void buck2obj_free(struct buck2obj_buf *buf);

int buck2obj_parse(struct buck2obj_buf *buf, uns buck_type, uns buck_len, struct fastbuf *body, struct odes *o_hdr, uns *body_start, struct odes *o_body);
struct odes *obj_read_bucket(struct buck2obj_buf *buf, struct mempool *pool, uns buck_type, uns buck_len, struct fastbuf *body, uns *body_start);
  /* If body_start != NULL, then only the header is parsed and *body_start is
   * set to the position of the body. This function does a plenty of optimizations
   * and if the body fastbuf is overwritable (body->can_overwrite_buffer), it can keep the
   * attribute values stored on their original locations in the fastbuf's buffer.
   * However, no such things are performed when reading the header only.
   */

int obj_read(struct fastbuf *, struct odes *);

/* obj2buck.c: Generating buckets from objects */

void put_attr_set_type(uns type);

uns size_attr(uns len);

byte *put_attr(byte *ptr, uns type, byte *val, uns len);
byte *put_attr_str(byte *ptr, uns type, byte *val);
byte *put_attr_vformat(byte *ptr, uns type, byte *mask, va_list va);
byte *put_attr_format(byte *ptr, uns type, char *mask, ...) __attribute__((format(printf,3,4)));
byte *put_attr_num(byte *ptr, uns type, uns val);
byte *put_attr_separator(byte *ptr);

void bput_attr(struct fastbuf *b, uns type, byte *val, uns len);
void bput_attr_str(struct fastbuf *b, uns type, byte *val);
void bput_attr_vformat(struct fastbuf *b, uns type, byte *mask, va_list va);
void bput_attr_format(struct fastbuf *b, uns type, char *mask, ...) __attribute__((format(printf,3,4)));
void bput_attr_num(struct fastbuf *b, uns type, uns val);
void bput_attr_separator(struct fastbuf *b);

void obj_write(struct fastbuf *, struct odes *);
void obj_write_nocheck(struct fastbuf *, struct odes *);

#endif
