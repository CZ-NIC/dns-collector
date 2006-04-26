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
#include "lib/fastbuf.h"
#include "lib/ipaccess.h"

#include <string.h>

struct addrmask {
  u32 addr;
  u32 mask;
};

struct ipaccess_entry {
  cnode n;
  int allow;
  struct addrmask addr;
};

static byte *
addrmask_parser(byte *c, void *ptr)
{
  /*
   * This is tricky: addrmasks will be compared by memcmp(), so we must ensure
   * that even the padding between structure members is zeroed out.
   */
  struct addrmask *am = ptr;
  bzero(am, sizeof(*am));

  byte *p = strchr(c, '/');
  if (p)
    *p++ = 0;
  byte *err = cf_parse_ip(c, &am->addr);
  if (err)
    return err;
  if (p)
    {
      uns len;
      if (!cf_parse_int(p, &len) && len <= 32)
	am->mask = ~(len == 32 ? 0 : ~0U >> len);
      else if (cf_parse_ip(p, &am->mask))
	return "Invalid prefix length or netmask";
    }
  else
    am->mask = ~0U;
  return NULL;
}

static void
addrmask_dumper(struct fastbuf *fb, void *ptr)
{
  struct addrmask *am = ptr;
  bprintf(fb, "%08x/%08x ", am->addr, am->mask);
}

static struct cf_user_type addrmask_type = {
  .size = sizeof(struct addrmask),
  .parser = addrmask_parser,
  .dumper = addrmask_dumper
};

struct cf_section ipaccess_cf = {
  CF_TYPE(struct ipaccess_entry),
  CF_ITEMS {
    CF_LOOKUP("Mode", PTR_TO(struct ipaccess_entry, allow), ((byte*[]) { "deny", "allow", NULL })),
    CF_USER("IP", PTR_TO(struct ipaccess_entry, addr), &addrmask_type),
    CF_END
  }
};

int
ipaccess_check(clist *l, u32 ip)
{
  CLIST_FOR_EACH(struct ipaccess_entry *, a, *l)
    if (! ((ip ^ a->addr.addr) & a->addr.mask))
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
