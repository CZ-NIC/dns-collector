/*
 *	UCW Library -- A simple XML parser -- Namespaces
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/gary.h>
#include <ucw-xml/xml.h>
#include <ucw-xml/internals.h>

/*
 *  This is an implementation of XML namespaces according to
 *  http://www.w3.org/TR/REC-xml-names/.
 *
 *  Currently, we assume that the document does not contain a plethora
 *  of namespaces and prefixes. So we keep them in memory until the
 *  document ends.
 */

struct ns_hash_entry {
  uint ns;
  char name[1];
};

#define HASH_NODE struct ns_hash_entry
#define HASH_PREFIX(x) ns_hash_##x
#define HASH_KEY_ENDSTRING name
#define HASH_WANT_CLEANUP
#define HASH_WANT_FIND
#define HASH_WANT_LOOKUP
#define HASH_TABLE_DYNAMIC
#define HASH_LOOKUP_DETECT_NEW
#define HASH_GIVE_ALLOC
XML_HASH_GIVE_ALLOC
#include <ucw/hashtable.h>

struct xml_ns_prefix {
  struct xml_ns_prefix *prev;
  struct xml_node *e;			/* Which element defined this prefix */
  struct ns_hash_entry *he;		/* NULL if changing default NS */
  uint prev_ns;				/* Previous NS ID assigned to this prefix */
};

static bool
ns_enabled(struct xml_context *ctx)
{
  return (ctx->flags & XML_NAMESPACES);
}

void
xml_ns_enable(struct xml_context *ctx)
{
  if (ns_enabled(ctx))
    return;

  TRACE(ctx, "NS: Enabling");
  ctx->flags |= XML_NAMESPACES;
  if (!ctx->ns_pool)
    {
      TRACE(ctx, "NS: Allocating data structures");
      ctx->ns_pool = mp_new(4096);
      GARY_INIT(ctx->ns_by_id, 16);
    }

  ctx->ns_by_name = xml_hash_new(ctx->ns_pool, sizeof(struct ns_hash_table));
  ns_hash_init(ctx->ns_by_name);

  ctx->ns_by_prefix = xml_hash_new(ctx->ns_pool, sizeof(struct ns_hash_table));
  ns_hash_init(ctx->ns_by_prefix);

  /* Intern well-known namespaces */
  GARY_RESIZE(ctx->ns_by_id, 0);
  uint none_ns = xml_ns_by_name(ctx, "");
  uint xmlns_ns = xml_ns_by_name(ctx, "http://www.w3.org/2000/xmlns/");
  uint xml_ns = xml_ns_by_name(ctx, "http://www.w3.org/XML/1998/namespace");
  ASSERT(none_ns == XML_NS_NONE && xmlns_ns == XML_NS_XMLNS && xml_ns == XML_NS_XML);

  /* Intern standard prefixes */
  int new_xmlns, new_xml;
  ns_hash_lookup(ctx->ns_by_prefix, "xmlns", &new_xmlns)->ns = xmlns_ns;
  ns_hash_lookup(ctx->ns_by_prefix, "xml", &new_xml)->ns = xml_ns;
  ASSERT(new_xmlns && new_xml);
}

void
xml_ns_cleanup(struct xml_context *ctx)
{
  if (!ctx->ns_pool)
    return;

  TRACE(ctx, "NS: Cleanup");
  ns_hash_cleanup(ctx->ns_by_prefix);
  ns_hash_cleanup(ctx->ns_by_name);
  GARY_FREE(ctx->ns_by_id);
  mp_delete(ctx->ns_pool);
}

void
xml_ns_reset(struct xml_context *ctx)
{
  if (!ns_enabled(ctx))
    return;

  TRACE(ctx, "NS: Reset");
  GARY_RESIZE(ctx->ns_by_id, 1);
  ctx->ns_by_id[0] = "";
  mp_flush(ctx->ns_pool);
}

const char *
xml_ns_by_id(struct xml_context *ctx, uint ns)
{
  ASSERT(ns < GARY_SIZE(ctx->ns_by_id));
  return ctx->ns_by_id[ns];
}

uint
xml_ns_by_name(struct xml_context *ctx, const char *name)
{
  int new_p;
  struct ns_hash_entry *he = ns_hash_lookup(ctx->ns_by_name, (char *) name, &new_p);
  if (new_p)
    {
      he->ns = GARY_SIZE(ctx->ns_by_id);
      ASSERT(he->ns < ~0U);
      *GARY_PUSH(ctx->ns_by_id) = he->name;
      TRACE(ctx, "NS: New namespace <%s> with ID %u", he->name, he->ns);
    }
  return he->ns;
}

static struct xml_ns_prefix *
ns_push_prefix(struct xml_context *ctx)
{
  struct xml_ns_prefix *px = mp_alloc(ctx->stack, sizeof(*px));
  px->prev = ctx->ns_prefix_stack;
  ctx->ns_prefix_stack = px;
  px->e = ctx->node;
  return px;
}

static uint
ns_resolve(struct xml_context *ctx, char **namep, uint default_ns)
{
  char *name = *namep;
  char *colon = strchr(name, ':');
  if (colon)
    {
      *colon = 0;
      struct ns_hash_entry *he = ns_hash_find(ctx->ns_by_prefix, name);
      *colon = ':';
      if (he && he->ns)
	{
	  *namep = colon + 1;
	  return he->ns;
	}
      else
	{
	  xml_error(ctx, "Unknown namespace prefix for %s", name);
	  return 0;
	}
    }
  else
    return default_ns;
}

void xml_ns_push_element(struct xml_context *ctx)
{
  struct xml_node *e = ctx->node;
  if (!ns_enabled(ctx))
    {
      e->ns = 0;
      return;
    }

  /* Scan attributes for prefix definitions */
  XML_ATTR_FOR_EACH(a, e)
    if (!memcmp(a->name, "xmlns", 5))
      {
	struct xml_ns_prefix *px = ns_push_prefix(ctx);
	uint ns = xml_ns_by_name(ctx, a->val);
	if (a->name[5] == ':')
	  {
	    if (a->name[6])
	      {
		/* New NS prefix */
		int new_p;
		struct ns_hash_entry *he = ns_hash_lookup(ctx->ns_by_prefix, a->name + 6, &new_p);
		if (new_p)
		  he->ns = 0;
		px->he = he;
		px->prev_ns = he->ns;
		he->ns = ns;
		TRACE(ctx, "NS: New prefix <%s> -> ID %u", he->name, he->ns);
	      }
	  }
	else
	  {
	    /* New default NS */
	    px->he = NULL;
	    px->prev_ns = ctx->ns_default;
	    ctx->ns_default = ns;
	    TRACE(ctx, "New default NS -> ID %u", ns);
	  }
      }

  /* Resolve namespaces */
  e->ns = ns_resolve(ctx, &e->name, ctx->ns_default);
  XML_ATTR_FOR_EACH(a, e)
    a->ns = ns_resolve(ctx, &a->name, 0);
}

void xml_ns_pop_element(struct xml_context *ctx)
{
  if (!ns_enabled(ctx))
    return;

  struct xml_ns_prefix *px;
  while ((px = ctx->ns_prefix_stack) && px->e == ctx->node)
    {
      struct ns_hash_entry *he = px->he;
      if (he)
	{
	  TRACE(ctx, "NS: Restoring prefix <%s> -> ID %u", he->name, px->prev_ns);
	  he->ns = px->prev_ns;
	}
      else
	{
	  TRACE(ctx, "NS: Restoring default NS -> ID %u", px->prev_ns);
	  ctx->ns_default = px->prev_ns;
	}
      ctx->ns_prefix_stack = px->prev;
    }
}
