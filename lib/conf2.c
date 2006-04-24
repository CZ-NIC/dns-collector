/*
 *	UCW Library -- Reading of configuration files
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf2.h"
#include "lib/mempool.h"
#include "lib/clists.h"
#include "lib/fastbuf.h"
#include "lib/chartype.h"
#include "lib/lfs.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <getopt.h>

#define TRY(f)	do { byte *_msg = f; if (_msg) return _msg; } while (0)

/* Memory allocation */

struct mempool *cf_pool;	// current pool for loading new configuration
static struct old_pools {
  struct old_pools *prev;
  struct mempool *pool;
} *pools;			// link-list of older cf_pool's

void *
cf_malloc(uns size)
{
  return mp_alloc(cf_pool, size);
}

void *
cf_malloc_zero(uns size)
{
  return mp_alloc_zero(cf_pool, size);
}

byte *
cf_strdup(byte *s)
{
  return mp_strdup(cf_pool, s);
}

byte *
cf_printf(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  byte *res = mp_vprintf(cf_pool, fmt, args);
  va_end(args);
  return res;
}

/* Undo journal */

uns cf_need_journal = 1;	// some programs do not need journal
static struct journal_item {
  struct journal_item *prev;
  byte *ptr;
  uns len;
  byte copy[0];
} *journal;

void
cf_journal_block(void *ptr, uns len)
{
  if (!cf_need_journal)
    return;
  struct journal_item *ji = cf_malloc(sizeof(struct journal_item) + len);
  ji->prev = journal;
  ji->ptr = ptr;
  ji->len = len;
  memcpy(ji->copy, ptr, len);
  journal = ji;
}

static void
journal_swap(void)
  // swaps the contents of the memory and the journal, and reverses the list
{
  struct journal_item *curr, *prev, *next;
  for (next=NULL, curr=journal; curr; next=curr, curr=prev)
  {
    prev = curr->prev;
    curr->prev = next;
    for (uns i=0; i<curr->len; i++)
    {
      byte x = curr->copy[i];
      curr->copy[i] = curr->ptr[i];
      curr->ptr[i] = x;
    }
  }
  journal = next;
}

static struct journal_item *
journal_new_transaction(uns new_pool)
{
  if (new_pool)
    cf_pool = mp_new(1<<10);
  struct journal_item *oldj = journal;
  journal = NULL;
  return oldj;
}

static void
journal_commit_transaction(uns new_pool, struct journal_item *oldj)
{
  if (new_pool)
  {
    struct old_pools *p = cf_malloc(sizeof(struct old_pools));
    p->prev = pools;
    p->pool = cf_pool;
    pools = p;
  }
  if (oldj)
  {
    struct journal_item **j = &journal;
    while (*j)
      j = &(*j)->prev;
    *j = oldj;
  }
}

static void
journal_rollback_transaction(uns new_pool, struct journal_item *oldj)
{
  if (!cf_need_journal)
    die("Cannot rollback the configuration, because the journal is disabled.");
  journal_swap();
  journal = oldj;
  if (new_pool)
  {
    mp_delete(cf_pool);
    cf_pool = pools ? pools->pool : NULL;
  }
}

/* Initialization */

#define SEC_FLAG_DYNAMIC	0x80000000	// contains a dynamic attribute
#define SEC_FLAG_UNKNOWN	0x40000000	// ignore unknown entriies
#define SEC_FLAG_NUMBER		0x0fffffff	// number of entries

static struct cf_section sections;	// root section

static struct cf_item *
find_subitem(struct cf_section *sec, byte *name)
{
  struct cf_item *ci = sec->cfg;
  for (; ci->cls; ci++)
    if (!strcasecmp(ci->name, name))
      return ci;
  return ci;
}

static void
inspect_section(struct cf_section *sec)
{
  sec->flags = 0;
  struct cf_item *ci;
  for (ci=sec->cfg; ci->cls; ci++)
    if (ci->cls == CC_SECTION) {
      inspect_section(ci->u.sec);
      sec->flags |= ci->u.sec->flags & SEC_FLAG_DYNAMIC;
    } else if (ci->cls == CC_LIST) {
      inspect_section(ci->u.sec);
      sec->flags |= SEC_FLAG_DYNAMIC;
    } else if (ci->cls == CC_DYNAMIC || ci->cls == CC_PARSER && ci->number < 0)
      sec->flags |= SEC_FLAG_DYNAMIC;
  sec->flags |= ci - sec->cfg;		// record the number of entries
}

void
cf_declare_section(byte *name, struct cf_section *sec, uns allow_unknown)
{
  if (!sections.cfg)
  {
    sections.size = 50;
    sections.cfg = xmalloc_zero(sections.size * sizeof(struct cf_item));
  }
  struct cf_item *ci = find_subitem(&sections, name);
  if (ci->cls)
    die("Cannot register section %s twice", name);
  ci->cls = CC_SECTION;
  ci->name = name;
  ci->number = 1;
  ci->ptr = NULL;
  ci->u.sec = sec;
  inspect_section(sec);
  if (allow_unknown)
    sec->flags |= SEC_FLAG_UNKNOWN;
  ci++;
  if (ci - sections.cfg >= (int) sections.size)
  {
    sections.cfg = xrealloc(sections.cfg, 2*sections.size * sizeof(struct cf_item));
    bzero(sections.cfg + sections.size, sections.size * sizeof(struct cf_item));
    sections.size *= 2;
  }
}

void
cf_init_section(byte *name, struct cf_section *sec, void *ptr, uns do_bzero)
{
  if (do_bzero) {
    ASSERT(sec->size);
    bzero(ptr, sec->size);
  }
  for (uns i=0; sec->cfg[i].cls; i++)
    if (sec->cfg[i].cls == CC_SECTION)
      cf_init_section(sec->cfg[i].name, sec->cfg[i].u.sec, ptr + (addr_int_t) sec->cfg[i].ptr, 0);
    else if (sec->cfg[i].cls == CC_LIST)
      clist_init(ptr + (addr_int_t) sec->cfg[i].ptr);
  if (sec->init) {
    byte *msg = sec->init(ptr);
    if (msg)
      die("Cannot initialize section %s: %s", name, msg);
  }
}

static void
global_init(void)
{
  static uns initialized = 0;
  if (initialized++)
    return;
  sections.flags |= SEC_FLAG_UNKNOWN;
  sections.size = 0;			// size of allocated array used to be stored here
  cf_init_section("top-level", &sections, NULL, 0);
}

static int
commit_section(byte *name, struct cf_section *sec, void *ptr)
{
  struct cf_item *ci;
  for (ci=sec->cfg; ci->cls; ci++)
    if (ci->cls == CC_SECTION) {
      if (commit_section(ci->name, ci->u.sec, ptr + (addr_int_t) ci->ptr)) {
	log(L_ERROR, "It happened in section %s", ci->name);
	return 1;
      }
    } else if (ci->cls == CC_LIST) {
      struct cnode *n;
      uns idx = 0;
      CLIST_WALK(n, * (clist*) (ptr + (addr_int_t) ci->ptr))
	if (idx++, commit_section(ci->name, ci->u.sec, n)) {
	  log(L_ERROR, "It happened in node #%d of list %s", idx, ci->name);
	  return 1;
	}
    }
  if (sec->commit) {
    byte *msg = sec->commit(ptr);
    if (msg) {
      log(L_ERROR, "Cannot commit section %s: %s", name, msg);
      return 1;
    }
  }
  return 0;
}

static struct cf_item *
find_item(struct cf_section *curr_sec, byte *name, byte **msg, void **ptr)
{
  *msg = NULL;
  if (name[0] == '^')				// absolute name instead of relative
    name++, curr_sec = &sections, *ptr = NULL;
  if (!curr_sec)				// don't even search in an unknown section
    return NULL;
  while (1)
  {
    byte *c = strchr(name, '.');
    if (c)
      *c++ = 0;
    struct cf_item *ci = find_subitem(curr_sec, name);
    if (!ci->cls)
    {
      if (!(curr_sec->flags & SEC_FLAG_UNKNOWN))	// ignore silently unknown top-level sections and unknown attributes in flagged sections
	*msg = cf_printf("Unknown item %s", name);
      return NULL;
    }
    *ptr += (addr_int_t) ci->ptr;
    if (!c)
      return ci;
    if (ci->cls != CC_SECTION)
    {
      *msg = cf_printf("Item %s is not a section", name);
      return NULL;
    }
    curr_sec = ci->u.sec;
    name = c;
  }
}

byte *
cf_find_item(byte *name, struct cf_item *item)
{
  byte *msg;
  void *ptr;
  struct cf_item *ci = find_item(&sections, name, &msg, &ptr);
  if (msg)
    return msg;
  *item = *ci;
  item->ptr = ptr;
  return NULL;
}

/* Safe loading and reloading */

static int load_file(byte *file);
static int load_string(byte *string);

int
cf_reload(byte *file)
{
  journal_swap();
  struct journal_item *oldj = journal_new_transaction(1);
  int err = load_file(file);
  if (!err)
  {
    for (struct old_pools *p=pools; p; p=pools)
    {
      pools = p->prev;
      mp_delete(p->pool);
    }
    journal_commit_transaction(1, NULL);
  }
  else
  {
    journal_rollback_transaction(1, oldj);
    journal_swap();
  }
  return err;
}

int
cf_load(byte *file)
{
  struct journal_item *oldj = journal_new_transaction(1);
  int err = load_file(file);
  if (!err)
    journal_commit_transaction(1, oldj);
  else
    journal_rollback_transaction(1, oldj);
  return err;
}

int
cf_set(byte *string)
{
  struct journal_item *oldj = journal_new_transaction(0);
  int err = load_string(string);
  if (!err)
    journal_commit_transaction(0, oldj);
  else
    journal_rollback_transaction(0, oldj);
  return err;
}

/* Parsers for standard types */

struct unit {
  uns name;			// one-letter name of the unit
  uns num, den;			// fraction
};

static const struct unit units[] = {
  { 'd', 86400, 1 },
  { 'h', 3600, 1 },
  { 'k', 1000, 1 },
  { 'm', 1000000, 1 },
  { 'g', 1000000000, 1 },
  { 'K', 1024, 1 },
  { 'M', 1048576, 1 },
  { 'G', 1073741824, 1 },
  { '%', 1, 100 },
  { 0, 0, 0 }
};

static const struct unit *
lookup_unit(byte *value, byte *end, byte **msg)
{
  if (end && *end) {
    if (end == value || end[1] || *end >= '0' && *end <= '9')
      *msg = "Invalid number";
    else {
      for (const struct unit *u=units; u->name; u++)
	if (u->name == *end)
	  return u;
      *msg = "Invalid unit";
    }
  }
  return NULL;
}

static char cf_rngerr[] = "Number out of range";

byte *
cf_parse_int(byte *str, int *ptr)
{
  byte *msg = NULL;
  if (!*str)
    msg = "Missing number";
  else {
    const struct unit *u;
    char *end;
    errno = 0;
    uns x = strtoul(str, &end, 0);
    if (errno == ERANGE)
      msg = cf_rngerr;
    else if (u = lookup_unit(str, end, &msg)) {
      u64 y = (u64)x * u->num;
      if (y % u->den)
	msg = "Number is not an integer";
      else {
	y /= u->den;
	if (y > 0xffffffff)
	  msg = cf_rngerr;
	*ptr = y;
      }
    } else
      *ptr = x;
  }
  return msg;
}

byte *
cf_parse_u64(byte *str, u64 *ptr)
{
  byte *msg = NULL;
  if (!*str)
    msg = "Missing number";
  else {
    const struct unit *u;
    char *end;
    errno = 0;
    u64 x = strtoull(str, &end, 0);
    if (errno == ERANGE)
      msg = cf_rngerr;
    else if (u = lookup_unit(str, end, &msg)) {
      if (x > ~(u64)0 / u->num)
	msg = "Number out of range";
      else {
	x *= u->num;
	if (x % u->den)
	  msg = "Number is not an integer";
	else
	  *ptr = x / u->den;
      }
    } else
      *ptr = x;
  }
  return msg;
}

byte *
cf_parse_double(byte *str, double *ptr)
{
  byte *msg = NULL;
  if (!*str)
    msg = "Missing number";
  else {
    const struct unit *u;
    double x;
    uns read_chars;
    if (sscanf(str, "%lf%n", &x, &read_chars) != 1)
      msg = "Invalid number";
    else if (u = lookup_unit(str, str + read_chars, &msg))
      *ptr = x * u->num / u->den;
    else
      *ptr = x;
  }
  return msg;
}

byte *
cf_parse_ip(byte *p, u32 *varp)
{
  if (!*p)
    return "Missing IP address";
  uns x = 0;
  char *p2;
  if (*p == '0' && p[1] | 32 == 'X') {
    errno = 0;
    x = strtoul(p + 2, &p2, 16);
    if (errno == ERANGE || p2 == (char*) (p+2) || x > 0xffffffff)
      goto error;
    p = p2;
  }
  else
    for (uns i = 0; i < 4; i++) {
      if (i) {
	if (*p++ != '.')
	  goto error;
      }
      errno = 0;
      uns y = strtoul(p, &p2, 10);
      if (errno == ERANGE || p2 == (char*) p || y > 255)
	goto error;
      p = p2;
      x = (x << 8) + y;
    }
  *varp = x;
  return *p ? "Trailing characters" : NULL;
error:
  return "Invalid IP address";
}

static byte *
cf_parse_string(byte *str, byte **ptr)
{
  *ptr = cf_strdup(str);
  return NULL;
}

/* Register size of and parser for each basic type */

typedef byte *cf_basic_parser(byte *str, void *ptr);
static struct {
  uns size;
  void *parser;
} parsers[] = {
  { sizeof(int), cf_parse_int },
  { sizeof(u64), cf_parse_u64 },
  { sizeof(double), cf_parse_double },
  { sizeof(u32), cf_parse_ip },
  { sizeof(byte*), cf_parse_string }
};

static byte *
cf_parse_ary(uns number, byte **pars, void *ptr, enum cf_type type)
{
  for (uns i=0; i<number; i++)
  {
    byte *msg = ((cf_basic_parser*) parsers[type].parser) (pars[i], ptr + i * parsers[type].size);
    if (msg)
      return cf_printf("Cannot parse item %d: %s", i+1, msg);
  }
  return NULL;
}

/* Interpreter */

#define T(x) #x,
static byte *op_names[] = { CF_OPERATIONS };
#undef T

#define OP_MASK 0xff		// only get the operation
#define OP_OPEN 0x100		// here we only get an opening brace instead of parameters
#define OP_1ST 0x200		// in the 1st phase selectors are recorded into the mask
#define OP_2ND 0x400		// in the 2nd phase real data are entered

static byte *
interpret_set_dynamic(struct cf_item *item, int number, byte **pars, void **ptr)
{
  enum cf_type type = item->u.type;
  cf_journal_block(ptr, sizeof(void*));
  // boundary checks done by the caller
  *ptr = cf_malloc((number+1) * parsers[type].size) + parsers[type].size;
  * (uns*) (*ptr - parsers[type].size) = number;
  return cf_parse_ary(number, pars, *ptr, type);
}

static byte *
interpret_add_dynamic(struct cf_item *item, int number, byte **pars, int *processed, void **ptr, enum cf_operation op)
{
  enum cf_type type = item->u.type;
  void *old_p = *ptr;
  int old_nr = * (int*) (old_p - parsers[type].size);
  int taken = MIN(number, item->number-old_nr);
  *processed = taken;
  // stretch the dynamic array
  void *new_p = cf_malloc((old_nr + taken + 1) * parsers[type].size) + parsers[type].size;
  * (uns*) (new_p - parsers[type].size) = old_nr + taken;
  cf_journal_block(ptr, sizeof(void*));
  *ptr = new_p;
  if (op == OP_APPEND) {
    memcpy(new_p, old_p, old_nr * parsers[type].size);
    return cf_parse_ary(taken, pars, new_p + old_nr * parsers[type].size, type);
  } else if (op == OP_PREPEND) {
    memcpy(new_p + taken * parsers[type].size, old_p, old_nr * parsers[type].size);
    return cf_parse_ary(taken, pars, new_p, type);
  } else
    return cf_printf("Dynamic arrays do not support operation %s", op_names[op]);
}

static byte *interpret_set_item(struct cf_item *item, int number, byte **pars, int *processed, void *ptr, uns allow_dynamic);

static byte *
interpret_section(struct cf_section *sec, int number, byte **pars, int *processed, void *ptr, uns allow_dynamic)
{
  *processed = 0;
  for (struct cf_item *ci=sec->cfg; ci->cls; ci++)
  {
    int taken;
    byte *msg = interpret_set_item(ci, number, pars, &taken, ptr + (addr_int_t) ci->ptr, allow_dynamic && !ci[1].cls);
    if (msg)
      return cf_printf("Item %s: %s", ci->name, msg);
    *processed += taken;
    number -= taken;
    pars += taken;
    if (!number)		// stop parsing, because many parsers would otherwise complain that number==0
      break;
  }
  return NULL;
}

static void
add_to_list(struct cnode *where, struct cnode *new_node, enum cf_operation op)
{
  switch (op)
  {
    case OP_EDIT:		// edition has been done in-place
      break;
    case OP_REMOVE:
      cf_journal_block(&where->prev->next, sizeof(void*));
      cf_journal_block(&where->next->prev, sizeof(void*));
      clist_remove(where);
      break;
    case OP_AFTER:		// implementation dependend (prepend_head = after(list)), and where==list, see clists.h:74
    case OP_PREPEND:
      cf_journal_block(&where->next->prev, sizeof(void*));
      cf_journal_block(&where->next, sizeof(void*));
      clist_insert_after(new_node, where);
      break;
    case OP_BEFORE:		// implementation dependend (append_tail = before(list))
    case OP_APPEND:
    case OP_SET:
      cf_journal_block(&where->prev->next, sizeof(void*));
      cf_journal_block(&where->prev, sizeof(void*));
      clist_insert_before(new_node, where);
      break;
    default:
      ASSERT(0);
  }
}

static byte *
interpret_add_list(struct cf_item *item, int number, byte **pars, int *processed, void *ptr, enum cf_operation op)
{
  if (op >= OP_REMOVE)
    return cf_printf("You have to open a block for operation %s", op_names[op]);
  if (!number)
    return "Nothing to add to the list";
  struct cf_section *sec = item->u.sec;
  *processed = 0;
  while (number > 0)
  {
    void *node = cf_malloc(sec->size);
    cf_init_section(item->name, sec, node, 1);
    add_to_list(ptr, node, op);
    int taken;
    /* If the node contains any dynamic attribute at the end, we suppress
     * auto-repetition here and pass the flag inside instead.  */
    TRY( interpret_section(sec, number, pars, &taken, node, sec->flags & SEC_FLAG_DYNAMIC) );
    *processed += taken;
    number -= taken;
    pars += taken;
    if (sec->flags & SEC_FLAG_DYNAMIC)
      break;
  }
  return NULL;
}

static byte *
interpret_set_item(struct cf_item *item, int number, byte **pars, int *processed, void *ptr, uns allow_dynamic)
{
  int taken;
  switch (item->cls)
  {
    case CC_STATIC:
      if (!number)
	return "Missing value";
      taken = MIN(number, item->number);
      *processed = taken;
      cf_journal_block(ptr, taken * parsers[item->u.type].size);
      return cf_parse_ary(taken, pars, ptr, item->u.type);
    case CC_DYNAMIC:
      if (!allow_dynamic)
	return "Dynamic array cannot be used here";
      taken = MIN(number, item->number);
      *processed = taken;
      return interpret_set_dynamic(item, taken, pars, ptr);
    case CC_PARSER:
      if (item->number < 0 && !allow_dynamic)
	return "Parsers with variable number of parameters cannot be used here";
      if (item->number > 0 && number < item->number)
	return "Not enough parameters available for the parser";
      taken = MIN(number, ABS(item->number));
      *processed = taken;
      for (int i=0; i<taken; i++)
	pars[i] = cf_strdup(pars[i]);
      return item->u.par(taken, pars, ptr);
    case CC_SECTION:
      return interpret_section(item->u.sec, number, pars, processed, ptr, allow_dynamic);
    case CC_LIST:
      if (!allow_dynamic)
	return "Lists cannot be used here";
      return interpret_add_list(item, number, pars, processed, ptr, OP_SET);
    default:
      ASSERT(0);
  }
}

static byte *
interpret_clear(struct cf_item *item, void *ptr)
{
  if (item->cls == CC_LIST) {
    cf_journal_block(ptr, sizeof(struct clist));
    clist_init(ptr);
  } else if (item->cls == CC_DYNAMIC) {
    cf_journal_block(ptr, sizeof(void *));
    * (void**) ptr = NULL;
  } else
    return "The item is not a list or a dynamic array";
  return NULL;
}

static int
cmp_items(void *i1, void *i2, struct cf_item *item)
{
  ASSERT(item->cls == CC_STATIC);
  i1 += (addr_int_t) item->ptr;
  i2 += (addr_int_t) item->ptr;
  if (item->u.type == CT_STRING)
    return strcmp(* (byte**) i1, * (byte**) i2);
  else				// all numeric types
    return memcmp(i1, i2, parsers[item->u.type].size);
}

static void *
find_list_node(struct clist *list, void *query, struct cf_section *sec, u32 mask)
{
  struct cnode *n;
  CLIST_WALK(n, *list)
  {
    uns found = 1;
    for (uns i=0; i<32; i++)
      if (mask & (1<<i))
	if (cmp_items(n, query, sec->cfg+i))
	{
	  found = 0;
	  break;
	}
    if (found)
      return n;
  }
  return NULL;
}

static byte *
record_selector(struct cf_item *item, struct cf_section *sec, u32 *mask)
{
  uns nr = sec->flags & SEC_FLAG_NUMBER;
  if (item >= sec->cfg && item < sec->cfg + nr)	// setting an attribute relative to this section
  {
    uns i = item - sec->cfg;
    if (i >= 32)
      return "Cannot select list nodes by this attribute";
    if (sec->cfg[i].cls != CC_STATIC)
      return "Selection can only be done based on basic attributes";
    *mask |= 1 << i;
  }
  return NULL;
}

#define MAX_STACK_SIZE	100
static struct item_stack {
  struct cf_section *sec;	// nested section
  void *base_ptr;		// because original pointers are often relative
  enum cf_operation op;		// it is performed when a closing brace is encountered
  void *list;			// list the operations should be done on
  u32 mask;			// bit array of selectors searching in a list
  struct cf_item *item;		// cf_item of the list
} stack[MAX_STACK_SIZE];
static uns level;

static byte *
opening_brace(struct cf_item *item, void *ptr, enum cf_operation op)
{
  if (level >= MAX_STACK_SIZE-1)
    return "Too many nested sections";
  stack[++level] = (struct item_stack) {
    .sec = NULL,
    .base_ptr = NULL,
    .op = op & OP_MASK,
    .list = NULL,
    .mask = 0,
    .item = NULL,
  };
  if (!item)			// unknown is ignored; we just need to trace recursion
    return NULL;
  stack[level].sec = item->u.sec;
  if (item->cls == CC_SECTION)
  {
    stack[level].base_ptr = ptr;
    stack[level].op = OP_EDIT | OP_2ND;	// this list operation does nothing
  }
  else if (item->cls == CC_LIST)
  {
    stack[level].base_ptr = cf_malloc(item->u.sec->size);
    cf_init_section(item->name, item->u.sec, stack[level].base_ptr, 1);
    stack[level].list = ptr;
    stack[level].item = item;
    if ((op & OP_MASK) < OP_REMOVE) {
      add_to_list(ptr, stack[level].base_ptr, op & OP_MASK);
      stack[level].op |= OP_2ND;
    } else
      stack[level].op |= OP_1ST;
  }
  else
    return "Opening brace can only be used on sections and lists";
  return NULL;
}

static byte *
closing_brace(struct item_stack *st, enum cf_operation op, int number, byte **pars)
{
  if (st->op == OP_CLOSE)	// top-level
    return "Unmatched } parenthesis";
  if (!st->sec) {		// dummy run on unknown section
    if (!(op & OP_OPEN))
      level--;
    return NULL;
  }
  enum cf_operation pure_op = st->op & OP_MASK;
  if (st->op & OP_1ST)
  {
    st->list = find_list_node(st->list, st->base_ptr, st->sec, st->mask);
    if (!st->list)
      return "Cannot find a node matching the query";
    if (pure_op != OP_REMOVE)
    {
      if (pure_op == OP_EDIT)
	st->base_ptr = st->list;
      else if (pure_op == OP_AFTER || pure_op == OP_BEFORE)
	cf_init_section(st->item->name, st->sec, st->base_ptr, 1);
      else
	ASSERT(0);
      if (op & OP_OPEN) {	// stay at the same recursion level
	st->op = (st->op | OP_2ND) & ~OP_1ST;
	add_to_list(st->list, st->base_ptr, pure_op);
	return NULL;
      }
      int taken;		// parse parameters on 1 line immediately
      TRY( interpret_section(st->sec, number, pars, &taken, st->base_ptr, 1) );
      number -= taken;
      pars += taken;
      // and fall-thru to the 2nd phase
    }
    add_to_list(st->list, st->base_ptr, pure_op);
  }
  level--;
  if (number)
    return "No parameters expected after the }";
  else if (op & OP_OPEN)
    return "No { is expected";
  else
    return NULL;
}

static byte *
interpret_line(byte *name, enum cf_operation op, int number, byte **pars)
{
  byte *msg;
  if ((op & OP_MASK) == OP_CLOSE)
    return closing_brace(stack+level, op, number, pars);
  void *ptr = stack[level].base_ptr;
  struct cf_item *item = find_item(stack[level].sec, name, &msg, &ptr);
  if (msg)
    return msg;
  if (stack[level].op & OP_1ST)
    TRY( record_selector(item, stack[level].sec, &stack[level].mask) );
  if (op & OP_OPEN)		// the operation will be performed after the closing brace
    return opening_brace(item, ptr, op);
  if (!item)			// ignored item in an unknown section
    return NULL;
  op &= OP_MASK;

  int taken;			// process as many parameters as possible
  if (op == OP_CLEAR)
    taken = 0, msg = interpret_clear(item, ptr);
  else if (op == OP_SET)
    msg = interpret_set_item(item, number, pars, &taken, ptr, 1);
  else if (item->cls == CC_DYNAMIC)
    msg = interpret_add_dynamic(item, number, pars, &taken, ptr, op);
  else if (item->cls == CC_LIST)
    msg = interpret_add_list(item, number, pars, &taken, ptr, op);
  else
    return cf_printf("Operation %s not supported on attribute %s", op_names[op], name);
  if (msg)
    return msg;
  if (taken < number)
    return cf_printf("Too many parameters: %d>%d", number, taken);

  return NULL;
}

byte *
cf_write_item(struct cf_item *item, enum cf_operation op, int number, byte **pars)
{
  byte *msg;
  int taken;
  switch (op) {
    case OP_SET:
      msg = interpret_set_item(item, number, pars, &taken, item->ptr, 1);
      break;
    case OP_CLEAR:
      taken = 0;
      msg = interpret_clear(item, item->ptr);
      break;
    case OP_APPEND:
    case OP_PREPEND:
      if (item->cls == CC_DYNAMIC)
	msg = interpret_add_dynamic(item, number, pars, &taken, item->ptr, op);
      else if (item->cls == CC_LIST)
	msg = interpret_add_list(item, number, pars, &taken, item->ptr, op);
      else
	return "The attribute class does not support append/prepend";
      break;
    default:
      return "Unsupported operation";
  }
  if (msg)
    return msg;
  if (taken < number)
    return "Too many parameters";
  return NULL;
}

static void
init_stack(void)
{
  global_init();
  level = 0;
  stack[0] = (struct item_stack) {
    .sec = &sections,
    .base_ptr = NULL,
    .op = OP_CLOSE,
    .list = NULL,
    .mask = 0,
    .item = NULL
  };
}

static int
done_stack(void)
{
  if (level > 0) {
    log(L_ERROR, "Unterminated block");
    return 1;
  }
  if (commit_section("top-level", &sections, NULL))
    return 1;
  return 0;
}

/* Text file parser */

static byte *name_parse_fb;
static struct fastbuf *parse_fb;
static uns line_num;

#define MAX_LINE	4096
static byte line_buf[MAX_LINE];
static byte *line = line_buf;

#include "lib/bbuf.h"
static bb_t copy_buf;
static uns copied;

#define GBUF_TYPE	uns
#define GBUF_PREFIX(x)	split_##x
#include "lib/gbuf.h"
static split_t word_buf;
static uns words;
static uns ends_by_brace;		// the line is ended by "{"

static int
get_line(void)
{
  if (!bgets(parse_fb, line_buf, MAX_LINE))
    return 0;
  line_num++;
  line = line_buf;
  while (Cblank(*line))
    line++;
  return 1;
}

static void
append(byte *start, byte *end)
{
  uns len = end - start;
  bb_grow(&copy_buf, copied + len + 1);
  memcpy(copy_buf.ptr + copied, start, len);
  copied += len + 1;
  copy_buf.ptr[copied-1] = 0;
}

#define	CONTROL_CHAR(x) (x == '{' || x == '}' || x == ';')
  // these characters separate words like blanks

static byte *
get_word(uns is_command_name)
{
  if (*line == '\'') {
    line++;
    while (1) {
      byte *start = line;
      while (*line && *line != '\'')
	line++;
      append(start, line);
      if (*line)
	break;
      copy_buf.ptr[copied-1] = '\n';
      if (!get_line())
	return "Unterminated apostrophe word at the end";
    }
    line++;

  } else if (*line == '"') {
    line++;
    uns start_copy = copied;
    while (1) {
      byte *start = line;
      uns escape = 0;
      while (*line) {
	if (*line == '"' && !escape)
	  break;
	else if (*line == '\\')
	  escape ^= 1;
	else
	  escape = 0;
	line++;
      }
      append(start, line);
      if (*line)
	break;
      if (!escape)
	copy_buf.ptr[copied-1] = '\n';
      else // merge two lines
	copied -= 2;
      if (!get_line())
	return "Unterminated quoted word at the end";
    }
    line++;

    // FIXME: this is utterly bogus, since printf() only expands these
    // percents, and the \n, \x1b codes are actually expanded by the compiler
    for (byte *c=copy_buf.ptr+start_copy; *c; c++)
      if (*c == '%') {
	if (c[1] != '%')
	  return "Formating sequences are not allowed";
	else
	  c++;
      }
    byte *tmp = cf_printf(copy_buf.ptr + start_copy);
    uns l = strlen(tmp);
    bb_grow(&copy_buf, start_copy + l + 1);
    strcpy(copy_buf.ptr + start_copy, tmp);
    copied = start_copy + l + 1;

  } else {
    // promised that *line is non-null and non-blank
    byte *start = line;
    while (*line && !Cblank(*line) && !CONTROL_CHAR(*line)
	&& (*line != '=' || !is_command_name))
      line++;
    if (*line == '=') {				// nice for setting from a command-line
      if (line == start)
	return "Assignment without a variable";
      *line = ' ';
    }
    if (line == start)				// already the first char is control
      line++;
    append(start, line);
  }
  while (Cblank(*line))
    line++;
  return NULL;
}

static byte *
get_token(uns is_command_name, byte **msg)
{
  *msg = NULL;
  while (1) {
    if (!*line || *line == '#') {
      if (!is_command_name || !get_line())
	return NULL;
    } else if (*line == ';') {
      *msg = get_word(0);
      if (!is_command_name || *msg)
	return NULL;
    } else if (*line == '\\' && !line[1]) {
      if (!get_line()) {
	*msg = "Last line ends by a backslash";
	return NULL;
      }
      if (!*line || *line == '#')
	log(L_WARN, "The line %s:%d following a backslash is empty", name_parse_fb, line_num);
    } else {
      split_grow(&word_buf, words+1);
      uns start = copied;
      word_buf.ptr[words++] = copied;
      *msg = get_word(is_command_name);
      return *msg ? NULL : copy_buf.ptr + start;
    }
  }
}

static byte *
split_command(void)
{
  words = copied = ends_by_brace = 0;
  byte *msg, *start_word;
  if (!(start_word = get_token(1, &msg)))
    return msg;
  if (*start_word == '{')			// only one opening brace
    return "Unexpected opening brace";
  while (*line != '}')				// stays for the next time
  {
    if (!(start_word = get_token(0, &msg)))
      return msg;
    if (*start_word == '{') {
      words--;					// discard the brace
      ends_by_brace = 1;
      break;
    }
  }
  return NULL;
}

/* Parsing multiple files */

static struct fastbuf *
bopen_safe(byte *name)
{
  int fd = sh_open(name, O_RDONLY);
  if (fd < 0) {
    log(L_ERROR, "Cannot open %s", name);
    return NULL;
  }
  return bopen(name, O_RDONLY, 1<<14);
}

static byte *
parse_fastbuf(byte *name_fb, struct fastbuf *fb, uns depth)
{
  byte *msg;
  name_parse_fb = name_fb;
  parse_fb = fb;
  line_num = 0;
  line = line_buf;
  *line = 0;
  while (1)
  {
    msg = split_command();
    if (msg)
      goto error;
    if (!words)
      return NULL;
    byte *name = copy_buf.ptr + word_buf.ptr[0];
    byte *pars[words-1];
    for (uns i=1; i<words; i++)
      pars[i-1] = copy_buf.ptr + word_buf.ptr[i];
    if (!strcasecmp(name, "include"))
    {
      if (words != 2)
	msg = "Expecting one filename";
      else if (depth > 8)
	msg = "Too many nested files";
      else if (*line && *line != '#')		// because the contents of line_buf is not re-entrant and will be cleared
	msg = "The input command must be the last one on a line";
      if (msg)
	goto error;
      struct fastbuf *new_fb = bopen_safe(pars[0]);
      if (!new_fb) {
	msg = "Cannot open file";
	goto error;
      }
      uns ll = line_num;
      msg = parse_fastbuf(pars[0], new_fb, depth+1);
      bclose(new_fb);
      if (msg)
	goto error;
      line_num = ll;
      parse_fb = fb;
    }
    enum cf_operation op;
    byte *c = strchr(name, ':');
    if (!c)
      op = strcmp(name, "}") ? OP_SET : OP_CLOSE;
    else {
      *c++ = 0;
      switch (Clocase(*c)) {
	case 's': op = OP_SET; break;
	case 'c': op = OP_CLEAR; break;
	case 'a': op = Clocase(c[1]) == 'p' ? OP_APPEND : OP_AFTER; break;
	case 'p': op = OP_PREPEND; break;
	case 'r': op = OP_REMOVE; break;
	case 'e': op = OP_EDIT; break;
	case 'b': op = OP_BEFORE; break;
	default: op = OP_SET; break;
      }
      if (strcasecmp(c, op_names[op])) {
	msg = cf_printf("Unknown operation %s", c);
	goto error;
      }
    }
    if (ends_by_brace)
      op |= OP_OPEN;
    msg = interpret_line(name, op, words-1, pars);
    if (msg)
      goto error;
  }
error:
  log(L_ERROR, "File %s, line %d: %s", name_fb, line_num, msg);
  return "included from here";
}

#ifndef DEFAULT_CONFIG
#define DEFAULT_CONFIG NULL
#endif
byte *cf_def_file = DEFAULT_CONFIG;

static int
load_file(byte *file)
{
  init_stack();
  struct fastbuf *fb = bopen_safe(file);
  if (!fb)
    return 1;
  byte *msg = parse_fastbuf(file, fb, 0);
  bclose(fb);
  int err = !!msg || done_stack();
  if (!err)
    cf_def_file = NULL;
  return err;
}

static int
load_string(byte *string)
{
  init_stack();
  struct fastbuf fb;
  fbbuf_init_read(&fb, string, strlen(string), 0);
  byte *msg = parse_fastbuf("memory string", &fb, 0);
  return !!msg || done_stack();
}

/* Command-line parser */

static void
load_default(void)
{
  if (cf_def_file)
    if (cf_load(cf_def_file))
      die("Cannot load default config %s", cf_def_file);
}

int
cf_get_opt(int argc, char * const argv[], const char *short_opts, const struct option *long_opts, int *long_index)
{
  static int other_options = 0;
  while (1) {
    int res = getopt_long (argc, argv, short_opts, long_opts, long_index);
    if (res == 'S' || res == 'C')
    {
      if (other_options)
	die("The -S and -C options must precede all other arguments");
      if (res == 'S') {
	load_default();
	if (cf_set(optarg))
	  die("Cannot set %s", optarg);
      } else {
	if (cf_load(optarg))
	  die("Cannot load %s", optarg);
      }
    } else {
      /* unhandled option or end of options */
      load_default();
      other_options++;
      return res;
    }
  }
}

/* Debug dumping */

static void
spaces(struct fastbuf *fb, uns nr)
{
  for (uns i=0; i<nr; i++)
    bputs(fb, "  ");
}

static void
dump_basic(struct fastbuf *fb, void *ptr, enum cf_type type)
{
  switch (type) {
    case CT_INT:	bprintf(fb, "%d ", *(uns*)ptr); break;
    case CT_U64:	bprintf(fb, "%llu ", *(u64*)ptr); break;
    case CT_DOUBLE:	bprintf(fb, "%lg ", *(double*)ptr); break;
    case CT_IP:		bprintf(fb, "%08x ", *(uns*)ptr); break;
    case CT_STRING:	bprintf(fb, "'%s' ", *(byte**)ptr); break;
  }
}

static void dump_section(struct fastbuf *fb, struct cf_section *sec, int level, void *ptr);

static void
dump_item(struct fastbuf *fb, struct cf_item *item, int level, void *ptr)
{
  ptr += (addr_int_t) item->ptr;
  enum cf_type type = item->u.type;
  int i;
  spaces(fb, level);
  bprintf(fb, "%s: c%d #%d ", item->name, item->cls, item->number);
  if (item->cls == CC_STATIC || item->cls == CC_DYNAMIC)
    bprintf(fb, "t%d ", type);
  if (item->cls == CC_STATIC) {
    for (i=0; i<item->number; i++)
      dump_basic(fb, ptr + i * parsers[type].size, type);
  } else if (item->cls == CC_DYNAMIC) {
    ptr = * (void**) ptr;
    if (ptr) {
      int real_nr = * (int*) (ptr - parsers[type].size);
      bprintf(fb, "##%d ", real_nr);
      for (i=0; i<real_nr; i++)
	dump_basic(fb, ptr + i * parsers[type].size, type);
    } else
      bprintf(fb, "NULL ");
  }
  bputc(fb, '\n');
  if (item->cls == CC_SECTION)
    dump_section(fb, item->u.sec, level+1, ptr);
  else if (item->cls == CC_LIST) {
    uns idx = 0;
    struct cnode *n;
    CLIST_WALK(n, * (clist*) ptr) {
      spaces(fb, level+1);
      bprintf(fb, "item %d\n", ++idx);
      dump_section(fb, item->u.sec, level+2, n);
    }
  }
}

static void
dump_section(struct fastbuf *fb, struct cf_section *sec, int level, void *ptr)
{
  spaces(fb, level);
  bprintf(fb, "S%d F%x:\n", sec->size, sec->flags);
  for (struct cf_item *item=sec->cfg; item->cls; item++)
    dump_item(fb, item, level, ptr);
}

void
cf_dump_sections(struct fastbuf *fb)
{
  dump_section(fb, &sections, 0, NULL);
}

/* TODO
 * - more space efficient journal
 * - don't commit all, but recognize from the journal which sections have been changed
 */
