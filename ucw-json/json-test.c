/*
 *	UCW JSON Library -- Tests
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>
#include <ucw/opt.h>
#include <ucw/trans.h>
#include <ucw-json/json.h>

static int opt_read;
static int opt_write;
static int opt_escape;
static int opt_indent;

static struct opt_section options = {
  OPT_ITEMS {
    OPT_HELP("Test program for UCW JSON library."),
    OPT_HELP("Usage: json-test [options]"),
    OPT_HELP(""),
    OPT_HELP("Options:"),
    OPT_HELP_OPTION,
    OPT_BOOL('r', "read", opt_read, 0, "\tRead JSON from standard input"),
    OPT_BOOL('w', "write", opt_write, 0, "\tWrite JSON to standard output"),
    OPT_BOOL('e', "escape", opt_escape, 0, "\tEscape non-ASCII characters in strings"),
    OPT_BOOL('i', "indent", opt_indent, 0, "\tIndent output"),
    OPT_END
  }
};

int main(int argc UNUSED, char **argv)
{
  opt_parse(&options, argv+1);

  struct json_context *js = json_new();
  struct json_node *n = NULL;

  if (opt_escape)
    js->format_options |= JSON_FORMAT_ESCAPE_NONASCII;
  if (opt_indent)
    js->format_options |= JSON_FORMAT_INDENT;

  if (opt_read)
    {
      struct fastbuf *fb = bfdopen_shared(0, 65536);
      TRANS_TRY
	{
	  n = json_parse(js, fb);
	}
      TRANS_CATCH(x)
	{
	  fprintf(stderr, "ERROR: %s\n", x->msg);
	  exit(1);
	}
      TRANS_END;
      bclose(fb);
    }

  if (!n)
    {
      n = json_new_number(js, 42);
    }

  if (opt_write)
    {
      struct fastbuf *fb = bfdopen_shared(1, 65536);
      json_write(js, fb, n);
      bclose(fb);
    }

  json_delete(js);
  return 0;
}
