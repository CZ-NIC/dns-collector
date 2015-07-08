/*
 *	UCW JSON Library -- Formatter
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>
#include <ucw/gary.h>
#include <ucw/ff-unicode.h>
#include <ucw/unicode.h>
#include <ucw-json/json.h>

#include <stdio.h>

void json_set_output(struct json_context *js, struct fastbuf *fb)
{
  js->out_fb = fb;
}

static void write_string(struct fastbuf *fb, const char *p)
{
  bputc(fb, '"');
  for (;;)
    {
      uint u;
      p = utf8_32_get(p, &u);
      if (!u)
	break;
      if (u == '"' || u == '\\')
	{
	  bputc(fb, '\\');
	  bputc(fb, u);
	}
      else if (u < 0x20)
	{
	  // We avoid "\f" nor "\b" and use "\uXXXX" instead
	  switch (u)
	    {
	    case 0x09: bputs(fb, "\\t"); break;
	    case 0x0a: bputs(fb, "\\n"); break;
	    case 0x0d: bputs(fb, "\\r"); break;
	    default:
	      bprintf(fb, "\\u%04x", u);
	    }
	}
      else
	bputc(fb, u);
    }
  bputc(fb, '"');
}

void json_write_value(struct json_context *js, struct json_node *n)
{
  struct fastbuf *fb = js->out_fb;

  switch (n->type)
    {
    case JSON_NULL:
      bputs(fb, "null");
      break;
    case JSON_BOOLEAN:
      bputs(fb, (n->boolean ? "true" : "false"));
      break;
    case JSON_NUMBER:
      // FIXME: Formatting of floats
      bprintf(fb, "%f", n->number);
      break;
    case JSON_STRING:
      write_string(fb, n->string);
      break;
    case JSON_ARRAY:
      {
	// FIXME: Indent
	bputs(fb, "[ ");
	for (size_t i=0; i < GARY_SIZE(n->elements); i++)
	  {
	    if (i)
	      bputs(fb, ", ");
	    json_write_value(js, n->elements[i]);
	  }
	bputc(fb, ']');
	break;
      }
    case JSON_OBJECT:
      {
	bputs(fb, "{ ");
	// FIXME: Indent
	for (size_t i=0; i < GARY_SIZE(n->pairs); i++)
	  {
	    if (i)
	      bputs(fb, ", ");
	    struct json_pair *p = &n->pairs[i];
	    write_string(fb, p->key);
	    bputs(fb, ": ");
	    json_write_value(js, p->value);
	  }
	bputc(fb, '}');
	break;
      }
    default:
      ASSERT(0);
    }
}

void json_write(struct json_context *js, struct fastbuf *fb, struct json_node *n)
{
  json_set_output(js, fb);
  json_write_value(js, n);
  bputc(fb, '\n');
}
