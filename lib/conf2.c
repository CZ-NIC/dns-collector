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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

uns cf_need_journal;		// some programs do not need journal
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
journal_new_section(uns new_pool)
{
  if (new_pool)
    cf_pool = mp_new(1<<14);
  struct journal_item *oldj = journal;
  journal = NULL;
  return oldj;
}

static void
journal_commit_section(uns new_pool, struct journal_item *oldj)
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
journal_rollback_section(uns new_pool, struct journal_item *oldj, byte *msg)
{
  if (!cf_need_journal)
    die("Cannot rollback the configuration, because the journal is disabled.  Error: %s", msg);
  journal_swap();
  journal = oldj;
  if (new_pool)
  {
    mp_delete(cf_pool);
    cf_pool = pools ? pools->pool : NULL;
  }
}

/* Initialization */

#define SEC_FLAG_DYNAMIC 0x80000000	// contains a dynamic attribute
#define SEC_FLAG_NUMBER 0x7fffffff	// number of entries

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
cf_declare_section(byte *name, struct cf_section *sec)
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
  ci++;
  if (ci - sections.cfg >= (int) sections.size)
  {
    sections.cfg = xrealloc(sections.cfg, 2*sections.size * sizeof(struct cf_item));
    bzero(sections.cfg + sections.size, sections.size * sizeof(struct cf_item));
    sections.size *= 2;
  }
}

void
cf_init_section(byte *name, struct cf_section *sec, void *ptr)
{
  if (sec->size)
    bzero(ptr, sec->size);
  for (uns i=0; sec->cfg[i].cls; i++)
    if (sec->cfg[i].cls == CC_SECTION)
      cf_init_section(sec->cfg[i].name, sec->cfg[i].u.sec, ptr + (addr_int_t) sec->cfg[i].ptr);
    else if (sec->cfg[i].cls == CC_LIST)
      clist_init(sec->cfg[i].ptr);
  byte *msg = sec->init(ptr);
  if (msg)
    die("Cannot initialize section %s: %s", name, msg);
}

static void
global_init(void)
{
  for (struct cf_item *ci=sections.cfg; ci->cls; ci++)
    cf_init_section(ci->name, ci->u.sec, NULL);
}

static struct cf_item *
find_item(struct cf_section *curr_sec, byte *name, byte **msg)
{
  *msg = NULL;
  if (name[0] == '^')				// absolute name instead of relative
    name++, curr_sec = &sections;
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
      if (curr_sec != &sections)		// ignore silently unknown top-level sections
	*msg = cf_printf("Unknown item %s", name);
      return NULL;
    }
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

/* Safe loading and reloading */

byte *cf_def_file = DEFAULT_CONFIG;

#ifndef DEFAULT_CONFIG
#define DEFAULT_CONFIG NULL
#endif

static byte *load_file(byte *file);
static byte *load_string(byte *string);

byte *
cf_reload(byte *file)
{
  journal_swap();
  struct journal_item *oldj = journal_new_section(1);
  byte *msg = load_file(file);
  if (!msg)
  {
    for (struct old_pools *p=pools; p; p=pools)
    {
      pools = p->prev;
      mp_delete(p->pool);
    }
    journal_commit_section(1, NULL);
  }
  else
  {
    journal_rollback_section(1, oldj, msg);
    journal_swap();
  }
  return msg;
}

byte *
cf_load(byte *file)
{
  struct journal_item *oldj = journal_new_section(1);
  byte *msg = load_file(file);
  if (!msg)
    journal_commit_section(1, oldj);
  else
    journal_rollback_section(1, oldj, msg);
  return msg;
}

byte *
cf_set(byte *string)
{
  struct journal_item *oldj = journal_new_section(0);
  byte *msg = load_string(string);
  if (!msg)
    journal_commit_section(0, oldj);
  else
    journal_rollback_section(0, oldj, msg);
  return msg;
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

//FIXME: parsers should handle well empty strings, unwanted suffixes etc.
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
    char *end;
    errno = 0;
    double x = strtoul(str, &end, 0);
    if (errno == ERANGE)
      msg = cf_rngerr;
    else if (u = lookup_unit(str, end, &msg))
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
  if (*p == '0' && p[1] | 32 == 'X') {
    errno = 0;
    x = strtoul(p + 2, NULL, 16);
    if (errno == ERANGE || x > 0xffffffff)
      goto error;
  }
  else
    for (uns i = 0; i < 4; i++) {
      if (i) {
	if (*p++ != '.')
	  goto error;
      }
      errno = 0;
      char *p2;
      uns y = strtoul(p, &p2, 10);
      p = p2;
      if (errno == ERANGE || y > 255)
	goto error;
      x = (x << 8) + y;
    }
  *varp = x;
  return NULL;
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

enum operation {
  OP_CLOSE,			// closing brace finishes previous block
  OP_CLEAR,			// list
  OP_SET,			// basic attribute (static, dynamic, parsed), section, list
  OP_APPEND,			// dynamic array, list
  OP_PREPEND,			// dynamic array, list
  OP_REMOVE,			// list operations with search follow
  OP_EDIT,
  OP_AFTER,
  OP_BEFORE
};
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
interpret_add_dynamic(struct cf_item *item, int number, byte **pars, int *processed, void **ptr, enum operation op)
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
    return cf_printf("Dynamic arrays do not support operation %d", op);
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
add_to_list(void *list, struct cnode *node, enum operation op)
{
  cf_journal_block(list, sizeof(struct clist));		//FIXME: we should journal the nodes of the list instead
  switch (op)
  {
    case OP_SET:
    case OP_APPEND:
      clist_add_tail(list, node);
      break;
    case OP_PREPEND:
      clist_add_head(list, node);
      break;
    case OP_REMOVE:
      clist_remove(list);
      break;
    case OP_EDIT:		// edition has been done in-place
      break;
    case OP_AFTER:		// here the pointer list is actually a pointer to another node
      clist_insert_after(node, list);
      break;
    case OP_BEFORE:
      clist_insert_before(node, list);
      break;
    default:
      ASSERT(0);
  }
}

static byte *
interpret_add_list(struct cf_item *item, int number, byte **pars, int *processed, void *ptr, enum operation op)
{
  if (!number)
    return "Missing value";
  ASSERT(op < OP_REMOVE);
  struct cf_section *sec = item->u.sec;
  *processed = 0;
  while (number > 0)
  {
    void *node = cf_malloc(sec->size);
    cf_init_section(item->name, sec, node);
    add_to_list(ptr, node, op);
    int taken;
    /* If the node contains any dynamic attribute at the end, we suppress
     * auto-repetition here and pass the flag inside instead.  */
    byte *msg = interpret_section(sec, number, pars, &taken, node, sec->flags & SEC_FLAG_DYNAMIC);
    if (msg)
      return msg;
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
      return interpret_add_list(item, number, pars, ptr, processed, OP_SET);
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
  enum operation op;		// it is performed when a closing brace is encountered
  void *list;			// list the operations should be done on
  u32 mask;			// bit array of selectors searching in a list
  struct cf_item *item;		// cf_item of the list
} stack[MAX_STACK_SIZE];
static uns level;

static byte *
opening_brace(struct cf_item *item, void *ptr, enum operation op)
{
  if (level >= MAX_STACK_SIZE-1)
    return "Too many nested sections";
  stack[++level] = (struct item_stack) {
    .sec = NULL,
    .base_ptr = NULL,
    .op = op,
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
    cf_init_section(item->name, item->u.sec, stack[level].base_ptr);
    stack[level].list = ptr;
    stack[level].item = item;
    stack[level].op |= op < OP_REMOVE ? OP_2ND : OP_1ST;
  }
  else
    return "Opening brace can only be used on sections and lists";
  return NULL;
}

static byte *
closing_brace(struct item_stack *st, int number, byte **pars)
{
  if (st->op == OP_CLOSE)	// top-level
    return "Unmatched } parenthese";
  if (!st->sec) {		// dummy run on unknown section
    level--;
    return NULL;
  }
  enum operation op = st->op & OP_MASK;
  if (st->op & OP_1ST)
  {
    st->list = find_list_node(st->list, st->base_ptr, st->sec, st->mask);
    if (!st->list)
      return "Cannot find a node matching the query";
    if (op != OP_REMOVE)
    {
      if (op == OP_EDIT)
	st->base_ptr = st->list;
      else if (op == OP_AFTER || op == OP_BEFORE)
	cf_init_section(st->item->name, st->sec, st->base_ptr);
      else
	ASSERT(0);
      if (st->op & OP_OPEN) {	// stay at the same recursion level
	st->op = (st->op | OP_2ND) & ~OP_1ST;
	return NULL;
      }
      int taken;		// parse parameters on 1 line immediately
      byte *msg = interpret_section(st->sec, number, pars, &taken, st->base_ptr, 1);
      if (msg)
	return msg;
      number -= taken;
      pars += taken;
      // and fall-thru to the 2nd phase
    }
  }
  add_to_list(st->list, st->base_ptr, op);
  level--;
  if (number)
    return "No parameters expected after the }";
  else if (st->op & OP_OPEN)
    return "No { is expected";
  else
    return NULL;
}

static byte *
interpret_line(byte *name, enum operation op, int number, byte **pars)
{
  byte *msg;
  if (op == OP_CLOSE)
    return closing_brace(stack+level, number, pars);
  struct cf_item *item = find_item(stack[level].sec, name, &msg);
  if (msg)
    return msg;
  if (stack[level].op & OP_1ST) {
    msg = record_selector(item, stack[level].sec, &stack[level].mask);
    if (msg)
      return msg;
  }
  void *ptr = stack[level].base_ptr + (addr_int_t) item->ptr;
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
    return cf_printf("Operation %d not supported on attribute class %d", op, item->cls);
  if (msg)
    return msg;
  if (taken < number)
    return cf_printf("Too many parameters: %d>%d", number, taken);

  return NULL;
}

