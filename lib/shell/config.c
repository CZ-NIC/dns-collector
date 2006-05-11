/*
 *	UCW Library -- Shell Interface to Configuration Files
 *
 *	(c) 2002--2005 Martin Mares <mj@ucw.cz>
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	Once we were using this beautiful Shell version, but it turned out
 *	that it doesn't work with nested config files:
 *
 *		eval `sed <cf/sherlock '/^#/d;/^ *$/d;s/ \+$//;
 *		h;s@[^ 	]*@@;x;s@[ 	].*@@;y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/;G;s/\n//;
 *		/^\[SECTION\]/,/^\[/ {; /^[A-Z]/ { s/^\([^ 	]\+\)[ 	]*\(.*\)$/SH_\1="\2"/; p; }; };
 *		d;'`
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/getopt.h"
#include "lib/clists.h"
#include "lib/mempool.h"
#include "lib/chartype.h"
#include "lib/bbuf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>

static void
help(void)
{
  fputs("\n\
Usage: config [-C<configfile>] [-S<section>.<option>=<value>] <sections>\n\
\n\
<sections>\t<section>[,<sections>]\n\
<section>\t[*]<name>{[<items>]}\n\
<items>\t\t<item>[,<items>]\n\
<item>\t\t<static> | @<list>\n\
<static>\t[-]<type><name>\n\
<list>\t\t<name>{[<items>]}\n\
\n\
Types:\n\
<empty>\t\tString\n\
#\t\t32-bit integer\n\
##\t\t64-bit integer\n\
$\t\tFloating point number\n\
\n\
Modifiers:\n\
*\t\tReport unknown items as errors\n\
-\t\tDo not dump item's value\n\
", stderr);
  exit(1);
}

union value {
  void *v_ptr;
  int v_int;
  u64 v_u64;
  double v_double;
  clist list;
};

#define FLAG_HIDE		0x1
#define FLAG_NO_UNKNOWN		0x2

struct item {
  cnode node;
  uns flags;
  struct cf_item cf;
  union value value;
};

struct section {
  struct item item;
  clist list;
  uns count;
  uns size;
};

static struct mempool *pool;
static clist sections;
static byte *pos;

static void
parse_white(void)
{
  while (Cspace(*pos))
    pos++;
}

static void
parse_char(byte c)
{
  if (*pos++ != c)
    die("Missing '%c'", c);
}

static byte *
parse_name(void)
{
  byte *name = pos;
  while (Cword(*pos))
    pos++;
  uns len = pos - name;
  if (!len)
    die("Expected item/section name");
  byte *buf = mp_alloc(pool, len + 1);
  memcpy(buf, name, len);
  buf[len] = 0;
  return buf;
}

static void
parse_section(struct section *section)
{
  for (uns sep = 0; ; sep = 1)
    {
      parse_white();
      if (!*pos || *pos == '}')
	break;
      if (sep)
	parse_char(',');
      parse_white();

      struct item *item;

      if (*pos == '@')
        {
	  pos++;
	  struct section *sec = mp_alloc_zero(pool, sizeof(*sec));
	  sec->size = sizeof(cnode);
	  clist_init(&sec->list);
	  item = &sec->item;
	  item->cf.name = parse_name();
	  item->cf.cls = CC_LIST;
	  item->cf.number = 1;
	  parse_white();
	  parse_char('{');
	  parse_section(sec);
	  parse_char('}');
	}
      else
        {
	  item = mp_alloc_zero(pool, sizeof(*item));
	  if (*pos == '-')
	    {
	      item->flags |= FLAG_HIDE;
	      pos++;
	    }
	  item->cf.cls = CC_STATIC;
	  item->cf.number = 1;
	  switch (*pos)
	    {
	      case '#':
		if (*++pos == '#')
		  {
		    pos++;
		    item->cf.type = CT_U64;
		  }
		else
		  item->cf.type = CT_INT;
		break;
	      case '$':
		pos++;
		item->cf.type = CT_DOUBLE;
		break;
	      default:
		if (!Cword(*pos))
		  die("Invalid type syntax");
		item->cf.type = CT_STRING;
		break;
	    }
	  item->cf.name = parse_name();
	}
      if (section->item.cf.cls == CC_LIST)
        {
          item->cf.ptr = (void *)section->size;
          section->size += sizeof(union value);
        }
      else
        item->cf.ptr = &item->value;
      clist_add_tail(&section->list, &item->node);
      section->count++;
    }
}

static void
parse_outer(void)
{
  for (uns sep = 0; ; sep = 1)
    {
      parse_white();
      if (!*pos)
	break;
      if (sep)
	parse_char(',');
      parse_white();
      struct section *sec = mp_alloc_zero(pool, sizeof(*sec));
      if (*pos == '*')
        {
	  pos++;
	  sec->item.flags |= FLAG_NO_UNKNOWN;
	}
      sec->item.cf.name = parse_name();
      parse_white();
      parse_char('{');
      clist_add_tail(&sections, &sec->item.node);
      clist_init(&sec->list);
      parse_section(sec);
      parse_char('}');
    }
}

static struct cf_section *
generate_section(struct section *section)
{
  struct cf_section *sec = mp_alloc_zero(pool, sizeof(*sec));
  if (section->item.cf.cls == CC_LIST)
    sec->size = section->size;
  struct cf_item *c = sec->cfg = mp_alloc_zero(pool, sizeof(struct cf_item) * (section->count + 1));
  CLIST_FOR_EACH(struct item *, item, section->list)
    {
      *c = item->cf;
      if (c->cls == CC_LIST)
	c->u.sec = generate_section((struct section *)item);
      c++;
    }
  c->cls = CC_END;
  return sec;
}

static bb_t path;

static void
dump_item(struct item *item, void *ptr, uns path_len)
{
  if (item->flags & FLAG_HIDE)
    return;
  union value *val = (union value *)((addr_int_t)ptr + (addr_int_t)item->cf.ptr);
  if (item->cf.cls == CC_LIST)
    {
      uns len = strlen(item->cf.name);
      bb_grow(&path, path_len + len + 1);
      path.ptr[path_len] = '_';
      memcpy(path.ptr + path_len + 1, item->cf.name, len);
      CLIST_FOR_EACH(cnode *, ptr2, val->list)
        CLIST_FOR_EACH(struct item *, item2, ((struct section *)item)->list)
          dump_item(item2, ptr2, path_len + len + 1);
    }
  else
    {
      byte *name = item->cf.name;
      byte buf[128], *value = buf;
      bb_grow(&path, path_len + 1)[path_len] = 0;
      if (!ptr)
        printf("CF_%s_%s='", path.ptr, name);
      else
        printf("CF_%s_%s[${#CF_%s_%s[*]}]='", path.ptr, name, path.ptr, name);
      switch (item->cf.type)
        {
          case CT_INT:
	    sprintf(buf, "%d", val->v_int);
            break;
          case CT_U64:
	    sprintf(buf, "%Lu", val->v_u64);
	    break;
	  case CT_DOUBLE:
	    sprintf(buf, "%g", val->v_double);
	    break;
	  case CT_STRING:
	    if (val->v_ptr)
	      value = val->v_ptr;
	    else
	      *value = 0;
	    break;
	  default:
	    ASSERT(0);
	}
          while (*value) {
#if 0
            if (*value == '\'')
	      die("Apostrophes are not supported in config of scripts");
#endif
            if (*value == '\'')
	      printf("'\\''");
	    else
              putchar(*value);
	    value++;
          }
	  printf("'\n");
    }
}

int main(int argc, char **argv)
{
  log_init("config");
  if (argc < 2)
    help();
  pos = argv[argc - 1];
  argv[argc - 1] = NULL;

  pool = mp_new(0x1000);
  clist_init(&sections);
  parse_outer();
  CLIST_FOR_EACH(struct section *, sec, sections)
    cf_declare_section(sec->item.cf.name, generate_section(sec), !(sec->item.flags & FLAG_NO_UNKNOWN));

  if (cf_getopt(argc - 1, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) != -1)
    help();

  bb_init(&path);
  CLIST_FOR_EACH(struct section *, section, sections)
    {
      uns len = strlen(section->item.cf.name);
      memcpy(bb_grow(&path, len), section->item.cf.name, len);
      CLIST_FOR_EACH(struct item *, item, section->list)
        dump_item(item, NULL, len);
    }
  bb_done(&path);

  return 0;
}

