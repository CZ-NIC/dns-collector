/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "sherlock/sherlock.h"
#include "sherlock/xml/xml.h"
#include "lib/getopt.h"
#include "lib/fastbuf.h"

#include <stdio.h>
#include <stdlib.h>

static char *shortopts = "sp" CF_SHORT_OPTS;
static struct option longopts[] = {
  CF_LONG_OPTS
  { "sax",	0, 0, 's' },
  { "pull",	0, 0, 'p' },
  { "dom",	0, 0, 'd' },
  { NULL,	0, 0, 0 }
};

static void NONRET
usage(void)
{
  fputs("\
Usage: xml-test [options] < in.xml\n\
\n\
Options:\n"
CF_USAGE
"\
-s, --pull  Test PULL interface\n\
-s, --sax   Test SAX interface\n\
-d, --dom   Test DOM interface\n\
\n", stderr);
  exit(1);
}

static uns want_sax;
static uns want_pull;
static uns want_dom;
static struct fastbuf *out;

static char *
node_type(struct xml_node *node)
{
  switch (node->type)
    {
      case XML_NODE_ELEM: return "element";
      case XML_NODE_COMMENT: return "comment";
      case XML_NODE_PI: return "pi";
      case XML_NODE_CDATA: return "chars";
      default: return "unknown";
    }
}

static void
show_node(struct xml_node *node)
{
  switch (node->type)
    {
      case XML_NODE_ELEM:
	bprintf(out, " <%s>", node->name);
        SLIST_FOR_EACH(struct xml_attr *, a, node->attrs)
          bprintf(out, " %s='%s'", a->name, a->val);
	bputc(out, '\n');
	break;
      case XML_NODE_COMMENT:
	bprintf(out, " text='%s'\n", node->text);
	break;
      case XML_NODE_PI:
	bprintf(out, " target=%s text='%s'\n", node->name, node->text);
	break;
      case XML_NODE_CDATA:
	bprintf(out, " text='%s'\n", node->text);
	break;
      default:
        bputc(out, '\n');
    }
}

static void
show_tree(struct xml_node *node, uns level)
{
  if (!node)
    return;
  bputs(out, "DOM:  ");
  for (uns i = 0; i < level; i++)
    bputs(out, "    ");
  bputs(out, node_type(node));
  show_node(node);
  if (node->type == XML_NODE_ELEM)
    CLIST_FOR_EACH(struct xml_node *, son, node->sons)
      show_tree(son, level + 1);
}

static void
h_error(struct xml_context *ctx)
{
  bprintf(out, "SAX:  %s at %u: %s\n", (ctx->err_code < XML_ERR_ERROR) ? "warn" : "error", xml_row(ctx), ctx->err_msg);
}

static void
h_document_start(struct xml_context *ctx UNUSED)
{
  bputs(out, "SAX:  document_start\n");
}

static void
h_document_end(struct xml_context *ctx UNUSED)
{
  bputs(out, "SAX:  document_end\n");
}

static void
h_xml_decl(struct xml_context *ctx)
{
  bprintf(out, "SAX:  xml_decl version=%s standalone=%d\n", ctx->version_str, ctx->standalone);
}

static void
h_doctype_decl(struct xml_context *ctx)
{
  bprintf(out, "SAX:  doctype_decl type=%s public='%s' system='%s' extsub=%d intsub=%d\n",
    ctx->document_type, ctx->eid.public_id ? : "", ctx->eid.system_id ? : "",
    !!(ctx->flags & XML_FLAG_HAS_EXTERNAL_SUBSET), !!(ctx->flags & XML_FLAG_HAS_INTERNAL_SUBSET));
}

static void
h_comment(struct xml_context *ctx)
{
  bputs(out, "SAX:  comment");
  show_node(ctx->node);
}

static void
h_pi(struct xml_context *ctx)
{
  bprintf(out, "SAX:  pi");
  show_node(ctx->node);
}

static void
h_element_start(struct xml_context *ctx)
{
  bprintf(out, "SAX:  element_start");
  show_node(ctx->node);
}

static void
h_element_end(struct xml_context *ctx)
{
  bprintf(out, "SAX:  element_end </%s>\n", ctx->node->name);
}

static void
h_chars(struct xml_context *ctx)
{
  bprintf(out, "SAX:  chars");
  show_node(ctx->node);
}

int
main(int argc, char **argv)
{
  int opt;
  cf_def_file = NULL; // FIXME 
  log_init(argv[0]);
  while ((opt = cf_getopt(argc, argv, shortopts, longopts, NULL)) >= 0)
    switch (opt)
      {
	case 's':
	  want_sax++;
	  break;
	case 'p':
	  want_pull++;
	  break;
	case 'd':
	  want_dom++;
	  break;
	default:
	  usage();
      }
  if (optind != argc)
    usage();

  out = bfdopen_shared(1, 4096);
  struct xml_context ctx;
  xml_init(&ctx);
  ctx.h_warn = ctx.h_error = ctx.h_fatal = h_error;
  if (want_sax)
    {
      ctx.h_document_start = h_document_start;
      ctx.h_document_end = h_document_end;
      ctx.h_xml_decl = h_xml_decl;
      ctx.h_doctype_decl = h_doctype_decl;
      ctx.h_comment = h_comment;
      ctx.h_pi = h_pi;
      ctx.h_element_start = h_element_start;
      ctx.h_element_end = h_element_end;
      ctx.h_chars = h_chars;
    }
  if (want_pull)
    ctx.want = XML_WANT_CHARS | XML_WANT_STAG | XML_WANT_ETAG | XML_WANT_COMMENT | XML_WANT_PI;
  if (want_dom)
    ctx.flags &= ~XML_DOM_FREE;
  xml_set_source(&ctx, bfdopen_shared(0, 4096));
  int state;
  bprintf(out, "PULL: start\n");
  while ((state = xml_next(&ctx)) >= 0 && state != XML_STATE_EOF)
    switch (state)
      {
	case XML_STATE_CHARS:
	  bprintf(out, "PULL: chars");
	  show_node(ctx.node);
	  break;
	case XML_STATE_STAG:
	  bprintf(out, "PULL: element_start");
	  show_node(ctx.node);
	  break;
	case XML_STATE_ETAG:
	  bprintf(out, "PULL: element_end </%s>\n", ctx.node->name);
	  break;
	case XML_STATE_COMMENT:
	  bprintf(out, "PULL: comment");
	  show_node(ctx.node);
	  break;
	case XML_STATE_PI:
	  bprintf(out, "PULL: pi");
	  show_node(ctx.node);
	  break;
#if 0
	case XML_STATE_CDATA:
	  bprintf(out, "PULL: cdata [%s]\n", ctx.node->text);
	  break;
#endif
      }
  if (state != XML_STATE_EOF)
    bprintf(out, "PULL: fatal error\n");
  else
    bprintf(out, "PULL: eof\n");

  if (want_dom)
    show_tree(ctx.root, 0);

  xml_cleanup(&ctx);
  bclose(out);
  return 0;
}
