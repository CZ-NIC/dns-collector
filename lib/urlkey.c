/*
 *	Sherlock Library -- URL Keys & URL Fingerprints
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/index.h"
#include "lib/url.h"
#include "lib/fastbuf.h"
#include "lib/chartype.h"
#include "lib/hashfunc.h"

#include <string.h>
#include <fcntl.h>

/*** Prefix recognition table ***/

struct pxtab_rhs {
  struct pxtab_node *node;
  uns len;
  byte rhs[1];
};

struct pxtab_node {
  struct pxtab_node *parent;
  struct pxtab_rhs *rhs;
  uns len, total_len;
  byte component[0];
};

#define HASH_NODE struct pxtab_node
#define HASH_PREFIX(p) pxtab_##p
#define HASH_KEY_COMPLEX(x) x parent, x component, x len
#define HASH_KEY_DECL struct pxtab_node *parent UNUSED, byte *component UNUSED, uns len UNUSED
#define HASH_WANT_FIND
#define HASH_WANT_LOOKUP
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_EXTRA_SIZE
#define HASH_GIVE_INIT_KEY
#define HASH_GIVE_ALLOC

static inline uns
pxtab_hash(HASH_KEY_DECL)
{
  return ((uns)parent) ^ hash_block(component, len);
}

static inline int
pxtab_eq(struct pxtab_node *p1, byte *c1, uns l1, struct pxtab_node *p2, byte *c2, uns l2)
{
  return p1 == p2 && l1 == l2 && !memcmp(c1, c2, l1);
}

static inline int
pxtab_extra_size(HASH_KEY_DECL)
{
  return len;
}

static inline void
pxtab_init_key(struct pxtab_node *node, HASH_KEY_DECL)
{
  node->parent = parent;
  node->len = len;
  memcpy(node->component, component, len);
  node->rhs = NULL;
}

static inline void *
pxtab_alloc(uns size)
{
  return cfg_malloc(size);
}

#include "lib/hashtable.h"

static inline byte *
pxtab_skip_first_comp(byte *x)
{
  while (*x && *x != ':')
    x++;
  byte *y = x;
  while (*x != '/' || x[1] != '/')
    {
      if (!*x)
	return y;
      x++;
    }
  return x+2;
}

static inline byte *
pxtab_skip_next_comp(byte *x)
{
  for(;;)
    {
      if (!*x)
	return x;
      if (*x == '/')
	return x+1;
      x++;
    }
}

static struct pxtab_node *
pxtab_find_rule(byte *lhs)
{
  byte *next;
  struct pxtab_node *node, *parent = NULL;

  next = pxtab_skip_first_comp(lhs);
  DBG("\tfirst: %.*s", next-lhs, lhs);
  node = pxtab_find(NULL, lhs, next-lhs);
  while (node && *next)
    {
      parent = node;
      lhs = next;
      next = pxtab_skip_next_comp(lhs);
      DBG("\tnext: %.*s", next-lhs, lhs);
      node = pxtab_find(parent, lhs, next-lhs);
    }
  return node ? : parent;
}

static struct pxtab_node *
pxtab_add_rule(byte *lhs, struct pxtab_rhs *rhs)
{
  byte *lhs_start = lhs;
  byte *next;
  struct pxtab_node *node, *parent;

  next = pxtab_skip_first_comp(lhs);
  DBG("\tfirst: %.*s", next-lhs, lhs);
  node = pxtab_lookup(NULL, lhs, next-lhs);
  for(;;)
    {
      if (node->rhs)
	return NULL;
      if (!*next)
	break;
      lhs = next;
      next = pxtab_skip_next_comp(lhs);
      parent = node;
      DBG("\tnext: %.*s", next-lhs, lhs);
      node = pxtab_lookup(parent, lhs, next-lhs);
    }
  DBG("\tsetting rhs, %d to eat", next-lhs_start);
  node->rhs = rhs;
  node->total_len = next - lhs_start;
  return node;
}

static struct pxtab_rhs *
pxtab_add_rhs(byte *rhs)
{
  uns len = strlen(rhs);
  struct pxtab_rhs *r = cfg_malloc(sizeof(*r) + len);
  r->len = len;
  memcpy(r->rhs, rhs, len+1);
  struct pxtab_node *node = pxtab_add_rule(rhs, r);
  r->node = node;
  return r;
}

static void
pxtab_load(byte *name)
{
  struct fastbuf *f;
  struct pxtab_rhs *rhs = NULL;
  byte line[MAX_URL_SIZE], url[MAX_URL_SIZE], *c, *d;
  int err;
  int lino = 0;

  DBG("Loading prefix table %s", name);
  f = bopen(name, O_RDONLY, 4096);
  while (bgets(f, line, sizeof(line)))
    {
      lino++;
      c = line;
      while (Cblank(*c))
	c++;
      if (!*c || *c == '#')
	continue;
      if (err = url_auto_canonicalize(c, url))
	die("%s, line %d: Invalid URL (%s)", name, lino, url_error(err));
      if (!(d = strrchr(c, '/')) || d[1])
	die("%s, line %d: Prefix rules must end with a slash", name, lino);
      if (c == line)
	{
	  DBG("Creating RHS <%s>", c);
	  if (!(rhs = pxtab_add_rhs(c)))
	    die("%s, line %d: Right-hand side already mapped", name, lino);
	}
      else if (!rhs)
	die("%s, line %d: Syntax error", name, lino);
      else
	{
	  DBG("Adding LHS <%s>", c);
	  if (!pxtab_add_rule(c, rhs))
	    die("%s, line %d: Duplicate rule", name, lino);
	}
    }
  bclose(f);
}

/*** Configuration ***/

static uns urlkey_www_hack;
static byte *urlkey_pxtab_path;

static struct cfitem urlkey_config[] = {
  { "URLKey",		CT_SECTION,	NULL },
  { "WWWHack",		CT_INT,		&urlkey_www_hack },
  { "PrefixTable",	CT_STRING,	&urlkey_pxtab_path },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR urlkey_conf_init(void)
{
  cf_register(urlkey_config);
}

void
url_key_init(uns load_prefixes)
{
  pxtab_init();
  if (load_prefixes && urlkey_pxtab_path)
    pxtab_load(urlkey_pxtab_path);
}

static inline byte *
url_key_remove_www(byte *url, byte **pbuf)
{
  if (urlkey_www_hack && !strncmp(url, "http://www.", 11))
    {
      byte *buf = *pbuf;
      strcpy(buf, "http://");
      strcpy(buf+7, url+11);
      DBG("\tWWW hack: %s -> %s", url, buf);
      url = buf;
      *pbuf = buf + MAX_URL_SIZE;
    }
  return url;
}

byte *
url_key(byte *url, byte *buf)
{
  DBG("Generating URL key for %s", url);
  url = url_key_remove_www(url, &buf);
  struct pxtab_node *rule = pxtab_find_rule(url);
  if (rule && rule->rhs && rule->rhs->node != rule)
    {
      struct pxtab_rhs *rhs = rule->rhs;
      DBG("\tApplying rule <%s>, remove %d, add %d", rhs->rhs, rule->total_len, rhs->len);
      memcpy(buf, rhs->rhs, rhs->len);
      strcpy(buf + rhs->len, url + rule->total_len);
      url = buf;
      buf += MAX_URL_SIZE;
    }
  url = url_key_remove_www(url, &buf);
  DBG("\tOutput: %s", url);
  return url;
}

void
url_fingerprint(byte *url, struct fingerprint *fp)
{
  byte buf[URL_KEY_BUF_SIZE];
  return fingerprint(url_key(url, buf), fp);
}
