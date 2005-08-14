/*
 *	UCW Library -- Shell Interface to Configuration Files
 *
 *	(c) 2002--2005 Martin Mares <mj@ucw.cz>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>

static struct cfitem *items;
static byte *flags;

enum flag {
  F_STRING = 0,
  F_INT = 1,
  F_DOUBLE = 2,
  F_INT64 = 3,
  F_TYPE_MASK = 7,
  F_ARRAY = 0x80,
};

static void
help(void)
{
  fputs("\n\
Usage: config [-C<configfile>] [-S<section>.<option>=<value>] <section> [@]<item><type>[=<default>] <item2> ... [*]\n\
\n\
Types:\n\
<empty>\t\tString\n\
#\t\t32-bit integer\n\
##\t\t64-bit integer\n\
$\t\tFloating point number\n\
\n\
Modifiers:\n\
@\t\tMultiple occurences of the item are passed as an array (otherwise the last one wins)\n\
*\t\tIgnore unknown items instead of reporting them as errors\n\
", stderr);
  exit(1);
}

static byte *
report(struct cfitem *item, byte *value)
{
  uns f = flags[item-items];
  byte *err;
  byte buf[128];

  if (f & F_ARRAY)
    printf("CF_%s[${#CF_%s[*]}]='", item->name, item->name);
  else
    printf("CF_%s='", item->name);

  switch (f & F_TYPE_MASK)
    {
    case F_STRING:
      break;
    case F_INT: ;
      uns val;
      if (err = cf_parse_int(value, &val))
	return err;
      sprintf(buf, "%d", val);
      value = buf;
      break;
    case F_INT64: ;
      u64 val64;
      if (err = cf_parse_u64(value, &val64))
	return err;
      sprintf(buf, "%Lu", val64);
      value = buf;
      break;
    case F_DOUBLE: ;
      double valf;
      if (err = cf_parse_double(value, &valf))
	return err;
      sprintf(buf, "%g", valf);
      value = buf;
      break;
    default:
      ASSERT(0);
    }
  while (*value)
    {
      if (*value == '\'')
	die("Apostrophes are not supported in config of scripts");
      putchar(*value++);
    }
  puts("'");
  return NULL;
}

int main(int argc, char **argv)
{
  int i = 1;
  int start;
  struct cfitem *c;

  log_init("config");
  while (i < argc && argv[i][0] == '-')
    i++;
  if (i + 1 >= argc)
    help();
  start = i;
  c = items = alloca(sizeof(struct cfitem) * (argc-i+1));
  flags = alloca(argc);
  bzero(flags, argc);
  c->name = argv[i];
  c->type = CT_SECTION;
  c->var = NULL;
  c++;
  while (++i < argc)
    {
      char *arg = xstrdup(argv[i]);
      if (!strcmp(arg, "*"))
	items->type = CT_INCOMPLETE_SECTION;
      else
	{
	  uns id = c-items;
	  char *e = strchr(arg, '=');
	  if (e)
	    *e++ = 0;

	  char *t = arg + strlen(arg) - 1;
	  if (t > arg)
	    {
	      if (*t == '#')
		{
		  *t-- = 0;
		  if (t > arg && *t == '#')
		    {
		      *t-- = 0;
		      flags[id] |= F_INT64;
		    }
		  else
		    flags[id] |= F_INT;
		}
	      else if (*t == '$')
		{
		  *t-- = 0;
		  flags[id] |= F_DOUBLE;
		}
	    }

	  if (*arg == '@')
	    {
	      arg++;
	      printf("declare -a CF_%s ; CF_%s=()\n", arg, arg);
	      flags[id] |= F_ARRAY;
	    }

	  c->type = CT_FUNCTION;
	  c->var = report;
	  c->name = arg;
	  if (e)
	    report(c, e);
	  c++;
	}
    }
  c->type = CT_STOP;
  cf_register(items);
  if (cf_getopt(start, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) != -1)
    help();
  return 0;
}
