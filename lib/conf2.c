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
  if (name[0] == '.')
    name++;
  else if (strchr(name, '.'))
    curr_sec = &sections;
  if (!curr_sec)
    return NULL;
  byte *c;
  while ((c = strchr(name, '.')))
  {
    *c++ = 0;
    struct cf_item *ci = find_subitem(curr_sec, name);
    if (ci->cls != CC_SECTION)
    {
      *msg = cf_printf("Item %s %s", name, !ci->cls ? "does not exist" : "is not a subsection");
      return NULL;
    }
    curr_sec = ci->u.sec;
    name = c;
  }
  struct cf_item *ci = find_subitem(curr_sec, name);
  if (!ci->cls)
  {
    *msg = "Unknown item";
    return NULL;
  }
  return ci;
}

/* Safe loading and reloading */

byte *cf_def_file = DEFAULT_CONFIG;

#ifndef DEFAULT_CONFIG
#define DEFAULT_CONFIG NULL
#endif

byte *cfdeffile = DEFAULT_CONFIG;

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
  OP_CLEAR,			// list
  OP_SET,			// basic attribute (static, dynamic, parsed), section, list
  OP_APPEND,			// dynamic array, list
  OP_PREPEND,			// dynamic array, list
  OP_REMOVE,			// list
  OP_OPEN = 0x80		// here we only have an opening brace
};

#define MAX_STACK_SIZE	100
static struct item_stack {
  struct cf_section *sec;	// nested section
  void *base_ptr;		// because original pointers are often relative
  enum operation op;		// it is performed when a closing brace is encountered
} stack[MAX_STACK_SIZE];
static uns level;

static byte *
parse_dynamic(struct cf_item *item, int number, byte **pars, void **ptr)
{
  enum cf_type type = item->u.type;
  if (number > item->number)
    return "Expecting shorter array";
  cf_journal_block(ptr, sizeof(void*));
  *ptr = cf_malloc((number+1) * parsers[type].size) + parsers[type].size;
  * (uns*) (*ptr - parsers[type].size) = number;
  return cf_parse_ary(number, pars, *ptr, type);
}

static byte *
add_to_dynamic(struct cf_item *item, int number, byte **pars, void **ptr, enum operation op)
{
  enum cf_type type = item->u.type;
  void *old_p = *ptr;
  int old_nr = * (int*) (old_p - parsers[type].size);
  if (old_nr + number > item->number)
    return "Cannot enlarge dynamic array";
  // stretch the dynamic array
  void *new_p = cf_malloc((old_nr + number + 1) * parsers[type].size) + parsers[type].size;
  * (uns*) (new_p - parsers[type].size) = old_nr + number;
  cf_journal_block(ptr, sizeof(void*));
  *ptr = new_p;
  if (op == OP_APPEND)
  {
    memcpy(new_p, old_p, old_nr * parsers[type].size);
    return cf_parse_ary(number, pars, new_p + old_nr * parsers[type].size, type);
  }
  else if (op == OP_PREPEND)
  {
    memcpy(new_p + number * parsers[type].size, old_p, old_nr * parsers[type].size);
    return cf_parse_ary(number, pars, new_p, type);
  }
  else
    ASSERT(0);
}

static byte *
parse_subsection(struct cf_section *sec, int number, byte **pars, void *ptr)
{
  struct cf_item *ci;
  for (ci=sec->cfg; ci->cls; ci++)
  {
    if (ci->cls == CC_DYNAMIC && !ci[1].cls)
      break;
    if (ci->cls != CC_STATIC)
      return "Only sections consisting entirely of basic attributes can be written on 1 line";
    if (number)
    {
      if (number < ci->number)
	return "The number of parameters does not fit the section attributes";
      void *p = ptr + (addr_int_t) ci->ptr;
      cf_journal_block(p, ci->number * parsers[ci->u.type].size);
      byte *msg = cf_parse_ary(ci->number, pars, p, ci->u.type);
      if (msg)
	return cf_printf("Attribute %s: %s", ci->name, msg);
      number -= ci->number;
      pars += ci->number;
    }
  }
  if (ci->cls == CC_DYNAMIC)
    return parse_dynamic(ci, number, pars, ptr + (addr_int_t) ci->ptr);
  else if (number)
    return "Too many parameters for this section";
  return NULL;
}

static void
add_to_list(struct clist *list, struct cnode *node, enum operation op)
{
  cf_journal_block(list, sizeof(struct clist));
  if (op == OP_APPEND || op == OP_SET)
    clist_add_tail(list, node);
  else if (op == OP_PREPEND)
    clist_add_head(list, node);
  else
    ASSERT(0);
}

static byte *
increase_stack(struct cf_item *item, enum operation op)
{
  if (level >= MAX_STACK_SIZE-1)
    return "Too many nested sections";
  ++level;
  if (item)			// fill in the base pointer
  {
    if (item->cls == CC_SECTION)
      stack[level].base_ptr = stack[level].base_ptr + (addr_int_t) item->ptr;
    else if (item->cls != CC_LIST)
    {
      stack[level].base_ptr = cf_malloc(item->u.sec->size);
      cf_init_section(item->name, item->u.sec, stack[level].base_ptr);
    }
    else
      return "Opening brace can only be used on sections and lists";
    stack[level].sec = item->u.sec;
  }
  else				// unknown is also handled here, since we need to trace recursion
  {
    stack[level].base_ptr = NULL;
    stack[level].sec = NULL;
  }
  stack[level].op = op;
  return NULL;
}

static byte *
interpret_item(byte *name, enum operation op, int number, byte **pars)
{
  byte *msg;
  struct cf_item *item = find_item(stack[level].sec, name, &msg);
  if (op & OP_OPEN)		// the operation will be performed after the closing brace
    return increase_stack(item, op) ? : msg;
  if (!item)
    return msg;

  void *ptr = stack[level].base_ptr + (addr_int_t) item->ptr;
  if (op == OP_CLEAR)		// clear link-list
  {
    if (item->cls != CC_LIST)
      return "The item is not a list";
    cf_journal_block(ptr, sizeof(struct clist));
    clist_init(ptr);
  }
  else if (op == OP_SET && item->cls != CC_LIST)
    switch (item->cls)		// setting regular variables
    {
      case CC_STATIC:
	if (number != item->number)
	  return item->number==1 ? "Expecting one scalar value, not an array" : "Expecting array of different length";
	cf_journal_block(ptr, number * parsers[item->u.type].size);
	return cf_parse_ary(number, pars, ptr, item->u.type);
      case CC_DYNAMIC:
	return parse_dynamic(item, number, pars, ptr);
      case CC_PARSER:
	if (item->number >= 0)
	{
	  if (number != item->number)
	    return "Expecting different number of parameters";
	} else {
	  if (number > -item->number)
	    return "Expecting less parameters";
	}
	for (int i=0; i<number; i++)
	  pars[i] = cf_strdup(pars[i]);
	return item->u.par(number, pars, ptr);
      case CC_SECTION:		// setting a subsection at once
	return parse_subsection(item->u.sec, number, pars, ptr);
      default:
	ASSERT(0);
    }
  else if (item->cls == CC_DYNAMIC)
    return add_to_dynamic(item, number, pars, ptr, op);
  else if (item->cls == CC_LIST)
  {				// adding to a list at once
    void *node = cf_malloc(item->u.sec->size);
    cf_init_section(item->name, item->u.sec, node);
    msg = parse_subsection(item->u.sec, number, pars, node);
    if (msg)
      return msg;
    add_to_list(ptr, node, op);
  }
  else
    ASSERT(0);
  return NULL;
}

