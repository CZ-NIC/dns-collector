/*
 *	Sherlock Library -- Shell Interface to Configuration Files
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
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

static void
help(void)
{
  die("Usage: config [-C<configfile>] [-S<section>.<option>=<value>] <section> <item>[=<default>] <item2> ... [*]");
}

int main(int argc, char **argv)
{
  int i = 1;
  int start;
  struct cfitem *items, *c;
  byte **vars;

  log_init("config");
  while (i < argc && argv[i][0] == '-')
    i++;
  if (i + 1 >= argc)
    help();
  start = i;
  c = items = alloca(sizeof(struct cfitem) * (argc-i+1));
  vars = alloca(sizeof(byte *) * argc);
  c->name = argv[i];
  c->type = CT_SECTION;
  c->var = NULL;
  c++;
  while (++i < argc)
    {
      if (!strcmp(argv[i], "*"))
	items->type = CT_INCOMPLETE_SECTION;
      else
	{
	  byte *e = strchr(argv[i], '=');
	  if (e)
	    *e++ = 0;
	  else
	    e = "";
	  c->name = argv[i];
	  c->type = CT_STRING;
	  c->var = &vars[i];
	  *(byte **)c->var = e;
	  c++;
	}
    }
  c->type = CT_STOP;
  cf_register(items);
  if (cf_getopt(start, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) != -1)
    help();

  c = items+1;
  while (c->type)
    {
      printf("CF_%s=\"%s\"\n", c->name, *(byte **)c->var);
      c++;
    }

  return 0;
}
