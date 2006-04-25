/*
 *	UCW Library -- IP address access lists
 *
 *	(c) 1997--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/clists.h"
#include "lib/conf2.h"
#include "lib/ipaccess.h"

#undef ipaccess_check			/* FIXME */

#include <string.h>

struct ipaccess_entry {
  cnode n;
  uns allow;
  u32 addr, mask;
};

static byte *
ipaccess_cf_ip(uns n UNUSED, byte **pars, struct ipaccess_entry *a)
{
  byte *c = pars[0];
  CF_JOURNAL_VAR(a->addr);
  CF_JOURNAL_VAR(a->mask);

  byte *p = strchr(c, '/');
  if (p)
    *p++ = 0;
  byte *err = cf_parse_ip(c, &a->addr);
  if (err)
    return err;
  if (p)
    {
      uns len;
      if (!cf_parse_int(p, &len) && len <= 32)
	a->mask = ~(len == 32 ? 0 : ~0U >> len);
      else if (cf_parse_ip(p, &a->mask))
	return "Invalid prefix length or netmask";
    }
  else
    a->mask = ~0U;
  return NULL;
}

static byte *
ipaccess_cf_mode(uns n UNUSED, byte **pars, struct ipaccess_entry *a)
{
  CF_JOURNAL_VAR(a->allow);
  if (!strcasecmp(pars[0], "allow"))
    a->allow = 1;
  else if (!strcasecmp(pars[0], "deny"))
    a->allow = 0;
  else
    return "Either `allow' or `deny' expected";
  return NULL;
}

struct cf_section ipaccess_cf = {
  CF_TYPE(struct ipaccess_entry),
  CF_ITEMS {
    CF_PARSER("Mode", NULL, ipaccess_cf_mode, 1),
    CF_PARSER("IP", NULL, ipaccess_cf_ip, 1),
    CF_END
  }
};

int
ipaccess_check(clist *l, u32 ip)
{
  CLIST_FOR_EACH(struct ipaccess_entry *, a, *l)
    if (! ((ip ^ a->addr) & a->mask))
      return a->allow;
  return 0;
}

#ifdef TEST

#include <stdio.h>

static clist t;

static struct cf_section test_cf = {
  CF_ITEMS {
    CF_LIST("A", &t, &ipaccess_cf),
    CF_END
  }
};

int main(int argc, char **argv)
{
  cf_declare_section("T", &test_cf, 0);
  if (cf_get_opt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) != -1)
    die("Invalid arguments");

  byte buf[256];
  while (fgets(buf, sizeof(buf), stdin))
    {
      byte *c = strchr(buf, '\n');
      if (c)
	*c = 0;
      u32 ip;
      if (cf_parse_ip(buf, &ip))
	puts("Invalid IP address");
      else if (ipaccess_check(&t, ip))
	puts("Allowed");
      else
	puts("Denied");
    }
  return 0;
}

#endif
