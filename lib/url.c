/*
 *	Sherlock Library -- URL Functions (according to RFC 1738 and 1808)
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/url.h"
#include "lib/chartype.h"
#include "lib/conf.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Configuration */

static uns url_ignore_spaces;
static uns url_ignore_underflow;
static byte *url_component_separators = "";
static uns url_min_repeat_count = 0x7fffffff;
static uns url_max_repeat_length = 0;

static struct cfitem url_config[] = {
  { "URL",		CT_SECTION,	NULL },
  { "IgnoreSpaces",	CT_INT,		&url_ignore_spaces },
  { "IgnoreUnderflow",	CT_INT,		&url_ignore_underflow },
  { "ComponentSeparators",	CT_STRING,	&url_component_separators },
  { "MinRepeatCount",		CT_INT,		&url_min_repeat_count },
  { "MaxRepeatLength",		CT_INT,		&url_max_repeat_length },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR url_init_config(void)
{
  cf_register(url_config);
}

/* Escaping and de-escaping */

static uns
enhex(uns x)
{
  return (x<10) ? (x + '0') : (x - 10 + 'A');
}

int
url_deescape(byte *s, byte *d)
{
  byte *dstart = d;
  byte *end = d + MAX_URL_SIZE - 10;
  while (*s)
    {
      if (d >= end)
	return URL_ERR_TOO_LONG;
      if (*s == '%')
	{
	  unsigned int val;
	  if (!Cxdigit(s[1]) || !Cxdigit(s[2]))
	    return URL_ERR_INVALID_ESCAPE;
	  val = Cxvalue(s[1])*16 + Cxvalue(s[2]);
	  if (val < 0x20)
	    return URL_ERR_INVALID_ESCAPED_CHAR;
	  switch (val)
	    {
	    case ';':
	      val = NCC_SEMICOLON; break;
	    case '/':
	      val = NCC_SLASH; break;
	    case '?':
	      val = NCC_QUEST; break;
	    case ':':
	      val = NCC_COLON; break;
	    case '@':
	      val = NCC_AT; break;
	    case '=':
	      val = NCC_EQUAL; break;
	    case '&':
	      val = NCC_AND; break;
	    case '#':
	      val = NCC_HASH; break;
	    }
	  *d++ = val;
	  s += 3;
	}
      else if (*s > 0x20)
	*d++ = *s++;
      else if (Cspace(*s))
	{
	  byte *s0 = s;
	  while (Cspace(*s))
	    s++;
	  if (!url_ignore_spaces || !(!*s || d == dstart))
	    {
	      while (Cspace(*s0))
		{
		  if (d >= end)
		    return URL_ERR_TOO_LONG;
		  *d++ = *s0++;
		}
	    }
	}
      else
	return URL_ERR_INVALID_CHAR;
    }
  *d = 0;
  return 0;
}

int
url_enescape(byte *s, byte *d)
{
  byte *end = d + MAX_URL_SIZE - 10;
  unsigned int c;

  while (c = *s)
    {
      if (d >= end)
	return URL_ERR_TOO_LONG;
      if (Calnum(c) ||							/* RFC 1738(2.2): Only alphanumerics ... */
	  c == '$' || c == '-' || c == '_' || c == '.' || c == '+' ||	/* ... and several other exceptions ... */
	  c == '!' || c == '*' || c == '\'' || c == '(' || c == ')' ||
	  c == ',' ||
	  c == '/' || c == '?' || c == ':' || c == '@' ||		/* ... and reserved chars used for reserved purpose */
	  c == '=' || c == '&' || c == '#' || c == ';')
	*d++ = *s++;
      else
	{
	  uns val = (*s < NCC_MAX) ? NCC_CHARS[*s] : *s;
	  *d++ = '%';
	  *d++ = enhex(val >> 4);
	  *d++ = enhex(val & 0x0f);
	  s++;
	}
    }
  *d = 0;
  return 0;
}

/* Split an URL (several parts may be copied to the destination buffer) */

byte *url_proto_names[URL_PROTO_MAX] = URL_PNAMES;
static int url_proto_path_flags[URL_PROTO_MAX] = URL_PATH_FLAGS;

uns
identify_protocol(byte *p)
{
  uns i;

  for(i=1; i<URL_PROTO_MAX; i++)
    if (!strcasecmp(p, url_proto_names[i]))
      return i;
  return URL_PROTO_UNKNOWN;
}

int
url_split(byte *s, struct url *u, byte *d)
{
  bzero(u, sizeof(struct url));
  u->port = ~0;
  u->bufend = d + MAX_URL_SIZE - 10;

  if (s[0] != '/')			/* Seek for "protocol:" */
    {
      byte *p = s;
      while (*p && Calnum(*p))
	p++;
      if (p != s && *p == ':')
	{
	  u->protocol = d;
	  while (s < p)
	    *d++ = *s++;
	  *d++ = 0;
	  u->protoid = identify_protocol(u->protocol);
	  s++;
	  if (url_proto_path_flags[u->protoid] && (s[0] != '/' || s[1] != '/'))
	    {
	      /* The protocol requires complete host spec, but it's missing -> treat as a relative path instead */
	      int len = d - u->protocol;
	      d -= len;
	      s -= len;
	      u->protocol = NULL;
	      u->protoid = 0;
	    }
	}
    }

  if (s[0] == '/')			/* Host spec or absolute path */
    {
      if (s[1] == '/')			/* Host spec */
	{
	  byte *q, *w, *e;
	  char *ep;

	  s += 2;
	  q = d;
	  while (*s && *s != '/')	/* Copy user:passwd@host:port */
	    *d++ = *s++;
	  *d++ = 0;
	  w = strchr(q, '@');
	  if (w)			/* user:passwd present */
	    {
	      *w++ = 0;
	      u->user = q;
	    }
	  else
	    w = q;
	  e = strchr(w, ':');
	  if (e)			/* host:port present */
	    {
	      uns p;
	      *e++ = 0;
	      p = strtoul(e, &ep, 10);
	      if (ep && *ep || p > 65535)
		return URL_ERR_INVALID_PORT;
	      else if (p)		/* Port 0 (e.g. in :/) is treated as default port */
		u->port = p;
	    }
	  u->host = w;
	}
    }

  u->rest = s;
  u->buf = d;
  return 0;
}

/* Normalization according to given base URL */

static uns std_ports[] = URL_DEFPORTS;	/* Default port numbers */

static int
relpath_merge(struct url *u, struct url *b)
{
  byte *a = u->rest;
  byte *o = b->rest;
  byte *d = u->buf;
  byte *e = u->bufend;
  byte *p;

  if (a[0] == '/')			/* Absolute path => OK */
    return 0;
  if (o[0] != '/')
    return URL_PATH_UNDERFLOW;

  if (!a[0])				/* Empty URL -> inherit everything */
    {
      u->rest = b->rest;
      return 0;
    }

  u->rest = d;				/* We know we'll need to copy the path somewhere else */

  if (a[0] == '#')			/* Another fragment */
    {
      for(p=o; *p && *p != '#'; p++)
	;
      goto copy;
    }
  if (a[0] == '?')			/* New query */
    {
      for(p=o; *p && *p != '#' && *p != '?'; p++)
	;
      goto copy;
    }
  if (a[0] == ';')			/* Change parameters */
    {
      for(p=o; *p && *p != ';' && *p != '?' && *p != '#'; p++)
	;
      goto copy;
    }

  p = NULL;				/* Copy original path and find the last slash */
  while (*o && *o != ';' && *o != '?' && *o != '#')
    {
      if (d >= e)
	return URL_ERR_TOO_LONG;
      if ((*d++ = *o++) == '/')
	p = d;
    }
  if (!p)
    return URL_ERR_REL_NOTHING;
  d = p;

  while (*a)
    {
      if (a[0] == '.')
	{
	  if (a[1] == '/' || !a[1])	/* Skip "./" and ".$" */
	    {
	      a++;
	      if (a[0])
		a++;
	      continue;
	    }
	  else if (a[1] == '.' && (a[2] == '/' || !a[2])) /* "../" */
	    {
	      a += 2;
	      if (a[0])
		a++;
	      if (d <= u->buf + 1)
		{
		  /*
		   * RFC 1808 says we should leave ".." as a path segment, but
		   * we intentionally break the rule and refuse the URL.
		   */
		  if (!url_ignore_underflow)
		    return URL_PATH_UNDERFLOW;
		}
	      else
		{
		  d--;			/* Discard trailing slash */
		  while (d[-1] != '/')
		    d--;
		}
	      continue;
	    }
	}
      while (a[0] && a[0] != '/')
	{
	  if (d >= e)
	    return URL_ERR_TOO_LONG;
	  *d++ = *a++;
	}
      if (a[0])
	*d++ = *a++;
    }

okay:
  *d++ = 0;
  u->buf = d;
  return 0;

copy:					/* Combine part of old URL with the new one */
  while (o < p)
    if (d < e)
      *d++ = *o++;
    else
      return URL_ERR_TOO_LONG;
  while (*a)
    if (d < e)
      *d++ = *a++;
    else
      return URL_ERR_TOO_LONG;
  goto okay;
}

int
url_normalize(struct url *u, struct url *b)
{
  int err;

  /* Basic checks */
  if (url_proto_path_flags[u->protoid] && !u->host ||
      u->host && !*u->host ||
      !u->host && u->user ||
      !u->rest)
    return URL_SYNTAX_ERROR;

  if (!u->protocol)
    {
      /* Now we know it's a relative URL. Do we have any base? */
      if (!b || !url_proto_path_flags[b->protoid])
	return URL_ERR_REL_NOTHING;
      u->protocol = b->protocol;
      u->protoid = b->protoid;

      /* Reference to the same host */
      if (!u->host)
	{
	  u->host = b->host;
	  u->user = b->user;
	  u->port = b->port;
	  if (err = relpath_merge(u, b))
	    return err;
	}
    }

  /* Fill in missing info */
  if (u->port == ~0U)
    u->port = std_ports[u->protoid];

  return 0;
}

/* Name canonicalization */

static void
lowercase(byte *b)
{
  if (b)
    while (*b)
      {
	if (*b >= 'A' && *b <= 'Z')
	  *b = *b + 0x20;
	b++;
      }
}

static void
kill_end_dot(byte *b)
{
  byte *k;

  if (b)
    {
      k = b + strlen(b) - 1;
      if (k > b && *k == '.')
	*k = 0;
    }
}

int
url_canonicalize(struct url *u)
{
  char *c;

  lowercase(u->protocol);
  lowercase(u->host);
  kill_end_dot(u->host);
  if ((!u->rest || !*u->rest) && url_proto_path_flags[u->protoid])
    u->rest = "/";
  if (u->rest && (c = strchr(u->rest, '#')))	/* Kill fragment reference */
    *c = 0;
  return 0;
}

/* Pack a broken-down URL */

static byte *
append(byte *d, byte *s, byte *e)
{
  if (d)
    while (*s)
      {
	if (d >= e)
	  return NULL;
	*d++ = *s++;
      }
  return d;
}

int
url_pack(struct url *u, byte *d)
{
  byte *e = d + MAX_URL_SIZE - 10;

  if (u->protocol)
    {
      d = append(d, u->protocol, e);
      d = append(d, ":", e);
      u->protoid = identify_protocol(u->protocol);
    }
  if (u->host)
    {
      d = append(d, "//", e);
      if (u->user)
	{
	  d = append(d, u->user, e);
	  d = append(d, "@", e);
	}
      d = append(d, u->host, e);
      if (u->port != std_ports[u->protoid] && u->port != ~0U)
	{
	  char z[10];
	  sprintf(z, "%d", u->port);
	  d = append(d, ":", e);
	  d = append(d, z, e);
	}
    }
  if (u->rest)
    d = append(d, u->rest, e);
  if (!d)
    return URL_ERR_TOO_LONG;
  *d = 0;
  return 0;
}

/* Error messages */

static char *errmsg[] = {
  "Something is wrong",
  "Too long",
  "Invalid character",
  "Invalid escape",
  "Invalid escaped character",
  "Invalid port number",
  "Relative URL not allowed",
  "Unknown protocol",
  "Syntax error",
  "Path underflow"
};

char *
url_error(uns err)
{
  if (err >= sizeof(errmsg) / sizeof(char *))
    err = 0;
  return errmsg[err];
}

/* A "macro" for canonical split */

int
url_canon_split(byte *u, byte *buf1, byte *buf2, struct url *url)
{
  int err;

  if (err = url_deescape(u, buf1))
    return err;
  if (err = url_split(buf1, url, buf2))
    return err;
  if (err = url_normalize(url, NULL))
    return err;
  return url_canonicalize(url);
}

/* Testing */

#ifdef TEST

int main(int argc, char **argv)
{
  char buf1[MAX_URL_SIZE], buf2[MAX_URL_SIZE], buf3[MAX_URL_SIZE], buf4[MAX_URL_SIZE];
  int err;
  struct url url, url0;

  if (argc != 2)
    return 1;
  if (err = url_deescape(argv[1], buf1))
    {
      printf("deesc: error %d\n", err);
      return 1;
    }
  printf("deesc: %s\n", buf1);
  if (err = url_split(buf1, &url, buf2))
    {
      printf("split: error %d\n", err);
      return 1;
    }
  printf("split: @%s@%s@%s@%d@%s\n", url.protocol, url.user, url.host, url.port, url.rest);
  if (err = url_split("http://mj@www.hell.org/123/sub_dir/index.html;param?query&zzz/subquery#fragment", &url0, buf3))
    {
      printf("split base: error %d\n", err);
      return 1;
    }
  if (err = url_normalize(&url0, NULL))
    {
      printf("normalize base: error %d\n", err);
      return 1;
    }
  printf("base: @%s@%s@%s@%d@%s\n", url0.protocol, url0.user, url0.host, url0.port, url0.rest);
  if (err = url_normalize(&url, &url0))
    {
      printf("normalize: error %d\n", err);
      return 1;
    }
  printf("normalize: @%s@%s@%s@%d@%s\n", url.protocol, url.user, url.host, url.port, url.rest);
  if (err = url_canonicalize(&url))
    {
      printf("canonicalize: error %d\n", err);
      return 1;
    }
  printf("canonicalize: @%s@%s@%s@%d@%s\n", url.protocol, url.user, url.host, url.port, url.rest);
  if (err = url_pack(&url, buf4))
    {
      printf("pack: error %d\n", err);
      return 1;
    }
  printf("pack: %s\n", buf4);
  if (err = url_enescape(buf4, buf2))
    {
      printf("enesc: error %d\n", err);
      return 1;
    }
  printf("enesc: %s\n", buf2);
  return 0;
}

#endif

struct component {
	byte *start;
	int length;
	u32 hash;
};

static inline u32
hashf(byte *start, int length)
{
	u32 hf = length;
	while (length-- > 0)
		hf = (hf << 8 | hf >> 24) ^ *start++;
	return hf;
}

static inline uns
repeat_count(struct component *comp, uns count, uns len)
{
	struct component *orig_comp = comp;
	uns found = 0;
	while (1)
	{
		uns i;
		comp += len;
		count -= len;
		found++;
		if (count < len)
			return found;
		for (i=0; i<len; i++)
			if (comp[i].hash != orig_comp[i].hash
			|| comp[i].length != orig_comp[i].length
			|| memcmp(comp[i].start, orig_comp[i].start, comp[i].length))
				return found;
	}
}

int
url_has_repeated_component(byte *url)
{
	struct component *comp;
	uns comps, comp_len, rep_prefix;
	byte *c;
	uns i;

	for (comps=0, c=url; c; comps++)
	{
		c = strpbrk(c, url_component_separators);
		if (c)
			c++;
	}
	if (comps < url_min_repeat_count)
		return 0;
	comp = alloca(comps * sizeof(struct component));
	for (i=0, c=url; c; i++)
	{
		comp[i].start = c;
		c = strpbrk(c, url_component_separators);
		if (c)
		{
			comp[i].length = c - comp[i].start;
			c++;
		}
		else
			comp[i].length = strlen(comp[i].start);
	}
	ASSERT(i == comps);
	for (i=0; i<comps; i++)
		comp[i].hash = hashf(comp[i].start, comp[i].length);
	for (comp_len = 1; comp_len <= url_max_repeat_length && comp_len <= comps; comp_len++)
		for (rep_prefix = 0; rep_prefix <= comps - comp_len; rep_prefix++)
			if (repeat_count(comp + rep_prefix, comps - rep_prefix, comp_len) >= url_min_repeat_count)
				return comp_len;
	return 0;
}
