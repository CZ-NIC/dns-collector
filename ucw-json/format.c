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

#include <float.h>
#include <stdio.h>

void json_set_output(struct json_context *js, struct fastbuf *fb)
{
  js->out_fb = fb;
}

static void write_string(struct json_context *js, const char *p)
{
  struct fastbuf *fb = js->out_fb;

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
      else if (u >= 0x007f && (js->format_options & JSON_FORMAT_ESCAPE_NONASCII))
	{
	  if (u < 0x10000)
	    bprintf(fb, "\\u%04x", u);
	  else if (u < 0x110000)
	    bprintf(fb, "\\u%04x\\u%04x", 0xd800 + ((u - 0x10000) >> 10), 0xdc00 + (u & 0x3ff));
	  else
	    ASSERT(0);
	}
      else
	bput_utf8_32(fb, u);
    }
  bputc(fb, '"');
}

static void write_number(struct fastbuf *fb, double val)
{
  bprintf(fb, "%.*g", DBL_DIG+1, val);
}

static bool want_indent_p(struct json_context *js)
{
  return (js->format_options & JSON_FORMAT_INDENT);
}

static void write_space(struct json_context *js)
{
  struct fastbuf *fb = js->out_fb;

  if (want_indent_p(js))
    {
      bputc(fb, '\n');
      for (uint i=0; i < js->out_indent; i++)
	bputc(fb, '\t');
    }
  else
    bputc(fb, ' ');
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
      write_number(fb, n->number);
      break;
    case JSON_STRING:
      write_string(js, n->string);
      break;
    case JSON_ARRAY:
      {
	if (!GARY_SIZE(n->elements))
	  bputs(fb, "[]");
	else
	  {
	    bputc(fb, '[');
	    js->out_indent++;
	    for (size_t i=0; i < GARY_SIZE(n->elements); i++)
	      {
		if (i)
		  bputc(fb, ',');
		write_space(js);
		json_write_value(js, n->elements[i]);
	      }
	    js->out_indent--;
	    write_space(js);
	    bputc(fb, ']');
	  }
	break;
      }
    case JSON_OBJECT:
      {
	if (!GARY_SIZE(n->pairs))
	  bputs(fb, "{}");
	else
	  {
	    bputc(fb, '{');
	    js->out_indent++;
	    for (size_t i=0; i < GARY_SIZE(n->pairs); i++)
	      {
		if (i)
		  bputc(fb, ',');
		write_space(js);
		struct json_pair *p = &n->pairs[i];
		write_string(js, p->key);
		bputs(fb, ": ");
		json_write_value(js, p->value);
	      }
	    js->out_indent--;
	    write_space(js);
	    bputc(fb, '}');
	  }
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
