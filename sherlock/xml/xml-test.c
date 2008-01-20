/*
 *	Sherlock Library -- A simple XML parser
 *
 *	(c) 2007--2008 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "sherlock/sherlock.h"
#include "sherlock/xml/xml.h"
#include "sherlock/xml/dtd.h"
#include "lib/getopt.h"
#include "lib/fastbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

enum {
  WANT_FIRST = 0x100,
  WANT_PARSE_DTD,
  WANT_HIDE_ERRORS,
  WANT_IGNORE_COMMENTS,
  WANT_IGNORE_PIS,
  WANT_REPORT_BLOCKS,
  WANT_FILE_ENTITIES,
};

static char *shortopts = "spd" CF_SHORT_OPTS;
static struct option longopts[] = {
  CF_LONG_OPTS
  { "sax",		0, 0, 's' },
  { "pull",		0, 0, 'p' },
  { "dom",		0, 0, 'd' },
  { "dtd",		0, 0, WANT_PARSE_DTD },
  { "hide-errors",	0, 0, WANT_HIDE_ERRORS },
  { "ignore-comments",	0, 0, WANT_IGNORE_COMMENTS },
  { "ignore-pis",	0, 0, WANT_IGNORE_PIS },
  { "reports-blocks",	0, 0, WANT_REPORT_BLOCKS },
  { "file-entities",	0, 0, WANT_FILE_ENTITIES },
  { NULL,		0, 0, 0 }
};

static void NONRET
usage(void)
{
  fputs("\
Usage: xml-test [options] < input.xml\n\
\n\
Options:\n"
CF_USAGE
"\
-p, --pull              Test PULL interface\n\
-s, --sax               Test SAX interface\n\
-d, --dom               Test DOM interface\n\
    --dtd               Enable parsing of DTD\n\
    --hide-errors       Hide warnings and error messages\n\
    --ignore-comments   Ignore processing instructions\n\
    --ignore-pis        Ignore comments\n\
    --report-blocks	Report blocks or characters and CDATA sections\n\
    --file-entities     Resolve file external entities (not fully normative)\n\
\n", stderr);
  exit(1);
}

static uns want_sax;
static uns want_pull;
static uns want_dom;
static uns want_parse_dtd;
static uns want_hide_errors;
static uns want_ignore_comments;
static uns want_ignore_pis;
static uns want_report_blocks;
static uns want_file_entities;

static struct fastbuf *out;

static char *
node_type(struct xml_node *node)
{
  switch (node->type)
    {
      case XML_NODE_ELEM: return "element";
      case XML_NODE_COMMENT: return "comment";
      case XML_NODE_PI: return "pi";
      case XML_NODE_CHARS: return "chars";
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
        XML_ATTR_FOR_EACH(a, node)
          bprintf(out, " %s='%s'", a->name, a->val);
	bputc(out, '\n');
	break;
      case XML_NODE_COMMENT:
	bprintf(out, " text='%s'\n", node->text);
	break;
      case XML_NODE_PI:
	bprintf(out, " target=%s text='%s'\n", node->name, node->text);
	break;
      case XML_NODE_CHARS:
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
    XML_NODE_FOR_EACH(son, node)
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
  bprintf(out, "SAX:  xml_decl version=%s standalone=%d fb_encoding=%s\n", ctx->version_str, ctx->standalone, ctx->src->fb_encoding);
}

static void
h_doctype_decl(struct xml_context *ctx)
{
  bprintf(out, "SAX:  doctype_decl type=%s public='%s' system='%s' extsub=%d intsub=%d\n",
    ctx->doctype, ctx->public_id ? : "", ctx->system_id ? : "",
    !!(ctx->flags & XML_HAS_EXTERNAL_SUBSET), !!(ctx->flags & XML_HAS_INTERNAL_SUBSET));
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
  bputs(out, "SAX:  pi");
  show_node(ctx->node);
}

static void
h_stag(struct xml_context *ctx)
{
  bputs(out, "SAX:  stag");
  show_node(ctx->node);
}

static void
h_etag(struct xml_context *ctx)
{
  bprintf(out, "SAX:  etag </%s>\n", ctx->node->name);
}

static void
h_chars(struct xml_context *ctx)
{
  bputs(out, "SAX:  chars");
  show_node(ctx->node);
}

static void
h_block(struct xml_context *ctx UNUSED, char *text, uns len UNUSED)
{
  bprintf(out, "SAX:  block text='%s'\n", text);
}

static void
h_cdata(struct xml_context *ctx UNUSED, char *text, uns len UNUSED)
{
  bprintf(out, "SAX:  cdata text='%s'\n", text);
}

static void
h_dtd_start(struct xml_context *ctx UNUSED)
{
  bputs(out, "SAX:  dtd_start\n");
}

static void
h_dtd_end(struct xml_context *ctx UNUSED)
{
  bputs(out, "SAX:  dtd_end\n");
}

static void
h_resolve_entity(struct xml_context *ctx, struct xml_dtd_entity *e)
{
  xml_push_fastbuf(ctx, bopen(e->system_id, O_RDONLY, 4096));
}

int
main(int argc, char **argv)
{
  int opt;
  cf_def_file = NULL;
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
	case WANT_PARSE_DTD:
	  want_parse_dtd++;
	  break;
	case WANT_HIDE_ERRORS:
	  want_hide_errors++;
	  break;
	case WANT_IGNORE_COMMENTS:
	  want_ignore_comments++;
	  break;
	case WANT_IGNORE_PIS:
	  want_ignore_pis++;
	  break;
	case WANT_REPORT_BLOCKS:
	  want_report_blocks++;
	  break;
	case WANT_FILE_ENTITIES:
	  want_file_entities++;
	  break;
	default:
	  usage();
      }
  if (optind != argc)
    usage();

  out = bfdopen_shared(1, 4096);
  struct xml_context ctx;
  xml_init(&ctx);
  if (!want_hide_errors)
    ctx.h_warn = ctx.h_error = ctx.h_fatal = h_error;
  if (want_sax)
    {
      ctx.h_document_start = h_document_start;
      ctx.h_document_end = h_document_end;
      ctx.h_xml_decl = h_xml_decl;
      ctx.h_doctype_decl = h_doctype_decl;
      ctx.h_comment = h_comment;
      ctx.h_pi = h_pi;
      ctx.h_stag = h_stag;
      ctx.h_etag = h_etag;
      ctx.h_chars = h_chars;
      if (want_report_blocks)
        {
          ctx.h_block = h_block;
          ctx.h_cdata = h_cdata;
	}
      ctx.h_dtd_start = h_dtd_start;
      ctx.h_dtd_end = h_dtd_end;
    }
  if (want_dom)
    ctx.flags |= XML_ALLOC_ALL;
  if (want_parse_dtd)
    ctx.flags |= XML_PARSE_DTD;
  if (want_ignore_comments)
    ctx.flags &= ~(XML_REPORT_COMMENTS | XML_ALLOC_COMMENTS);
  if (want_ignore_pis)
    ctx.flags &= ~(XML_REPORT_PIS | XML_ALLOC_PIS);
  if (want_file_entities)
    ctx.h_resolve_entity = h_resolve_entity;
  xml_push_fastbuf(&ctx, bfdopen_shared(0, 4096));
  bputs(out, "PULL: start\n");
  if (want_pull)
    {
      ctx.pull = XML_PULL_CHARS | XML_PULL_STAG | XML_PULL_ETAG | XML_PULL_COMMENT | XML_PULL_PI;
      uns state;
      while (state = xml_next(&ctx))
	switch (state)
	  {
	    case XML_STATE_CHARS:
	      bputs(out, "PULL: chars");
	      show_node(ctx.node);
	      break;
	    case XML_STATE_STAG:
	      bputs(out, "PULL: stag");
	      show_node(ctx.node);
	      break;
	    case XML_STATE_ETAG:
	      bprintf(out, "PULL: etag </%s>\n", ctx.node->name);
	      break;
	    case XML_STATE_COMMENT:
	      bputs(out, "PULL: comment");
	      show_node(ctx.node);
	      break;
	    case XML_STATE_PI:
	      bputs(out, "PULL: pi");
	      show_node(ctx.node);
	      break;
	    default:
	      bputs(out, "PULL: unknown\n");
	      break;
	  }
    }
  else
    xml_parse(&ctx);
  if (ctx.err_code)
    bprintf(out, "PULL: fatal error at %u: %s\n", xml_row(&ctx), ctx.err_msg);
  else
    {
      bputs(out, "PULL: eof\n");
      if (want_dom)
	show_tree(ctx.dom, 0);
    }

  xml_cleanup(&ctx);
  bclose(out);
  return 0;
}
