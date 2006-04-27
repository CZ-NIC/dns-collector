/*
 *	UCW Library -- Shell Interface to Configuration Files
 *
 *	(c) 2002--2005 Martin Mares <mj@ucw.cz>
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
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
#include "lib/conf2.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>

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
report(uns num, byte **pars, struct cf_item *item)
{
  uns f = item->type;
  for (uns i=0; i<num; i++)
  {
    byte buf[128];
    byte *err, *value = buf;
    switch (f & F_TYPE_MASK) {
      case F_STRING:
	value = pars[i];
	break;
      case F_INT:
	; uns val;
	if (err = cf_parse_int(pars[i], &val))
	  return err;
	sprintf(buf, "%d", val);
	break;
      case F_INT64:
	; u64 val64;
	if (err = cf_parse_u64(pars[i], &val64))
	  return err;
	sprintf(buf, "%Lu", val64);
	break;
      case F_DOUBLE:
	; double valf;
	if (err = cf_parse_double(pars[i], &valf))
	  return err;
	sprintf(buf, "%g", valf);
	break;
      default:
	ASSERT(0);
    }

    if (f & F_ARRAY)
      printf("CF_%s[${#CF_%s[*]}]='", item->name, item->name);
    else
      printf("CF_%s='", item->name);
    while (*value) {
      if (*value == '\'')
	die("Apostrophes are not supported in config of scripts");
      putchar(*value++);
    }
    puts("'");
  }
  return NULL;
}

int main(int argc, char **argv)
{
  int i = 1;
  int start;

  log_init("config");
  while (i < argc && argv[i][0] == '-')
    i++;
  if (i + 1 >= argc)
    help();
  start = i;

  byte *sec_name = argv[i++];
  uns allow_unknown = 0;
  struct cf_section *sec = xmalloc_zero(sizeof(struct cf_section));
  struct cf_item *c = sec->cfg = xmalloc_zero(sizeof(struct cf_item) * (argc-i+1));

  for (; i<argc; i++)
    {
      byte *arg = xstrdup(argv[i]);
      if (!strcmp(arg, "*"))
	allow_unknown = 1;
      else
	{
	  byte *e = strchr(arg, '=');
	  if (e)
	    *e++ = 0;

	  byte *t = arg + strlen(arg) - 1;
	  if (t > arg)
	    {
	      if (*t == '#')
		{
		  *t-- = 0;
		  if (t > arg && *t == '#')
		    {
		      *t-- = 0;
		      c->type |= F_INT64;
		    }
		  else
		    c->type |= F_INT;
		}
	      else if (*t == '$')
		{
		  *t-- = 0;
		  c->type |= F_DOUBLE;
		}
	    }

	  if (*arg == '@')
	    {
	      arg++;
	      printf("declare -a CF_%s ; CF_%s=()\n", arg, arg);
	      c->type |= F_ARRAY;
	    }

	  c->cls = CC_PARSER;
	  c->name = arg;
	  c->number = (c->type & F_ARRAY) ? CF_ANY_NUM : 1;
	  c->ptr = c;
	  c->u.par = (cf_parser*) report;
	  if (e)
	    report(1, &e, c);
	  c++;
	}
    }
  c->cls = CC_END;
  cf_declare_section(sec_name, sec, allow_unknown);
  if (cf_get_opt(start, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) != -1)
    help();
  return 0;
}
