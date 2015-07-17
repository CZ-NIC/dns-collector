/*
 *	UCW JSON Library -- Parser
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>
#include <ucw/ff-unicode.h>
#include <ucw/trans.h>
#include <ucw/unicode.h>
#include <ucw-json/json.h>

#include <errno.h>
#include <stdlib.h>

void json_set_input(struct json_context *js, struct fastbuf *in)
{
  js->in_fb = in;
  js->in_line = 1;
  js->in_column = 0;
  js->next_char = -1;
  js->next_token = NULL;
  js->in_eof = 0;
}

static void NONRET json_parse_error(struct json_context *js, const char *msg)
{
  trans_throw("ucw.json.parse", js, "%s at line %u:%u", msg, js->in_line, js->in_column);
}

static int json_get_char(struct json_context *js)
{
  int c = bget_utf8_32_repl(js->in_fb, -2);
  if (unlikely(c < 0))
    {
      if (c == -2)
	json_parse_error(js, "Malformed UTF-8 character");
      js->in_eof = 1;
      return c;
    }
  js->in_column++;
  return c;
}

static void json_unget_char(struct json_context *js, int c)
{
  js->next_char = c;
}

static struct json_node *json_triv_token(struct json_context *js, enum json_node_type type)
{
  js->trivial_token->type = type;
  return js->trivial_token;
}

static struct json_node *json_parse_number(struct json_context *js, int c)
{
  mp_push(js->pool);
  char *p = mp_start_noalign(js->pool, 0);

  // Optional minus
  if (c == '-')
    {
      p = mp_append_char(js->pool, p, c);
      c = json_get_char(js);
      if (!(c >= '0' && c <= '9'))
	json_parse_error(js, "Malformed number: just minus");
    }

  // Integer part
  if (c == '0')
    {
      // Leading zeroes are forbidden by RFC 7159
      p = mp_append_char(js->pool, p, c);
      c = json_get_char(js);
      if (c >= '0' && c <= '9')
	json_parse_error(js, "Malformed number: leading zero");
    }
  else
    {
      while (c >= '0' && c <= '9')
	{
	  p = mp_append_char(js->pool, p, c);
	  c = json_get_char(js);
	}
    }

  // Fractional part
  if (c == '.')
    {
      p = mp_append_char(js->pool, p, c);
      c = json_get_char(js);
      if (!(c >= '0' && c <= '9'))
	json_parse_error(js, "Malformed number: no digits after decimal point");
      while (c >= '0' && c <= '9')
	{
	  p = mp_append_char(js->pool, p, c);
	  c = json_get_char(js);
	}
    }

  // Exponent
  if (c == 'e' || c == 'E')
    {
      p = mp_append_char(js->pool, p, c);
      c = json_get_char(js);
      if (c == '+' || c == '-')
	{
	  p = mp_append_char(js->pool, p, c);
	  c = json_get_char(js);
	}
      if (!(c >= '0' && c <= '9'))
	json_parse_error(js, "Malformed number: empty exponent");
      while (c >= '0' && c <= '9')
	{
	  p = mp_append_char(js->pool, p, c);
	  c = json_get_char(js);
	}
    }

  json_unget_char(js, c);

  p = mp_end_string(js->pool, p);
  errno = 0;
  double val = strtod(p, NULL);
  if (errno == ERANGE)
    json_parse_error(js, "Number out of range");
  mp_pop(js->pool);

  return json_new_number(js, val);
}

static struct json_node *json_parse_name(struct json_context *js, int c)
{
  char name[16];
  uint i = 0;

  while (c >= 'a' && c <= 'z')
    {
      if (i < sizeof(name) - 1)
	name[i++] = c;
      c = json_get_char(js);
    }
  if (i >= sizeof(name) - 1)
    json_parse_error(js, "Invalid literal name");
  name[i] = 0;
  json_unget_char(js, c);

  struct json_node *n;
  if (!strcmp(name, "null"))
    n = json_new_null(js);
  else if (!strcmp(name, "false"))
    n = json_new_bool(js, 0);
  else if (!strcmp(name, "true"))
    n = json_new_bool(js, 1);
  else
    json_parse_error(js, "Invalid literal name");

  return n;
}

static uint json_parse_hex4(struct json_context *js)
{
  uint x = 0;
  for (int i=0; i<4; i++)
    {
      x = x << 4;
      int c = json_get_char(js);
      if (c >= '0' && c <= '9')
	x += c - '0';
      else if (c >= 'a' && c <= 'f')
	x += c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
	x += c - 'A' + 10;
      else
	json_parse_error(js, "Invalid Unicode escape sequence");
    }
  return x;
}

static struct json_node *json_parse_string(struct json_context *js, int c)
{
  char *p = mp_start_noalign(js->pool, 0);

  c = json_get_char(js);
  while (c != '"')
    {
      if (unlikely(c < 0x20))
	{
	  if (c < 0 || c == 0x0d || c == 0x0a)
	    json_parse_error(js, "Unterminated string");
	  else
	    json_parse_error(js, "Invalid control character in string");
	}
      if (unlikely(c >= 0xd800 && c < 0xf900))
	{
	  if (c < 0xe000)
	    json_parse_error(js, "Invalid surrogate character in string");
	  else
	    json_parse_error(js, "Invalid private-use character in string");
	}
      if (unlikely(c >= 0xf0000))
	{
	  if (c > 0x10ffff)
	    json_parse_error(js, "Invalid non-Unicode character in string");
	  else
	    json_parse_error(js, "Invalid private-use character in string");
	}
      if (c == '\\')
	{
	  c = json_get_char(js);
	  switch (c)
	    {
	    case '"':
	    case '\\':
	    case '/':
	      break;
	    case 'b':
	      c = 0x08;
	      break;
	    case 'f':
	      c = 0x0c;
	      break;
	    case 'n':
	      c = 0x0a;
	      break;
	    case 'r':
	      c = 0x0d;
	      break;
	    case 't':
	      c = 0x09;
	      break;
	    case 'u':
	      {
		uint x = json_parse_hex4(js);
		if (!x)
		  json_parse_error(js, "Zero bytes in strings are not supported");
		if (x >= 0xd800 && x < 0xf900)
		  {
		    if (x < 0xdc00)
		      {
			// High surrogate: low surrogate must follow
			uint y = 0;
			if (json_get_char(js) == '\\' && json_get_char(js) == 'u')
			  y = json_parse_hex4(js);
			if (!(y >= 0xdc00 && y < 0xe000))
			  json_parse_error(js, "Escaped high surrogate codepoint must be followed by a low surrogate codepoint");
			c = 0x10000 + ((x & 0x03ff) << 10) | (y & 0x03ff);
			if (c > 0xf0000)
			  json_parse_error(js, "Invalid escaped private-use character");
		      }
		    else if (x < 0xe000)
		      {
			// Low surrogate
			json_parse_error(js, "Invalid escaped surrogate codepoint");
		      }
		    else
		      json_parse_error(js, "Invalid escaped private-use character");
		  }
		else
		  c = x;
		break;
	      }
	    default:
	      json_parse_error(js, "Invalid backslash sequence in string");
	    }
	}
      p = mp_append_utf8_32(js->pool, p, c);
      c = json_get_char(js);
    }

  p = mp_end_string(js->pool, p);
  return json_new_string_ref(js, p);
}

static struct json_node *json_read_token(struct json_context *js)
{
  if (unlikely(js->in_eof))
    return json_triv_token(js, JSON_EOF);

  int c = js->next_char;
  if (c >= 0)
    js->next_char = -1;
  else
    c = json_get_char(js);

  while (c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d)
    {
      if (c == 0x0a)
	{
	  js->in_line++;
	  js->in_column = 0;
	}
      c = json_get_char(js);
    }
  if (c < 0)
    return json_triv_token(js, JSON_EOF);

  if (c >= '0' && c <= '9' || c == '-')
    return json_parse_number(js, c);

  if (c >= 'a' && c <= 'z')
    return json_parse_name(js, c);

  if (c == '"')
    return json_parse_string(js, c);

  switch (c)
    {
    case '[':
      return json_triv_token(js, JSON_BEGIN_ARRAY);
    case ']':
      return json_triv_token(js, JSON_END_ARRAY);
    case '{':
      return json_triv_token(js, JSON_BEGIN_OBJECT);
    case '}':
      return json_triv_token(js, JSON_END_OBJECT);
    case ':':
      return json_triv_token(js, JSON_NAME_SEP);
    case ',':
      return json_triv_token(js, JSON_VALUE_SEP);
    case '.':
      json_parse_error(js, "Numbers must start with a digit");
    case 0xfeff:
      json_parse_error(js, "Misplaced byte-order mark, complain in Redmond");
    default:
      json_parse_error(js, "Invalid character");
    }
}

struct json_node *json_peek_token(struct json_context *js)
{
  if (!js->next_token)
    js->next_token = json_read_token(js);
  return js->next_token;
}

struct json_node *json_next_token(struct json_context *js)
{
  struct json_node *t = js->next_token;
  if (t)
    {
      js->next_token = NULL;
      return t;
    }
  return json_read_token(js);
}

struct json_node *json_next_value(struct json_context *js)
{
  struct json_node *t = json_next_token(js);

  switch (t->type)
    {
    case JSON_EOF:
      return NULL;

    // Elementary values
    case JSON_NULL:
    case JSON_BOOLEAN:
    case JSON_NUMBER:
    case JSON_STRING:
      return t;

    // Array
    case JSON_BEGIN_ARRAY:
      {
	struct json_node *a = json_new_array(js);
	if (json_peek_token(js)->type == JSON_END_ARRAY)
	  json_next_token(js);
	else for (;;)
	  {
	    struct json_node *v = json_next_value(js);
	    if (!v)
	      json_parse_error(js, "Unterminated array");
	    json_array_append(a, v);

	    t = json_next_token(js);
	    if (t->type == JSON_END_ARRAY)
	      break;
	    if (t->type != JSON_VALUE_SEP)
	      json_parse_error(js, "Comma or right bracket expected");
	  }
	return a;
      }

    // Object
    case JSON_BEGIN_OBJECT:
      {
	struct json_node *o = json_new_object(js);
	if (json_peek_token(js)->type == JSON_END_OBJECT)
	  json_next_token(js);
	else for (;;)
	  {
	    struct json_node *k = json_next_value(js);
	    if (!k)
	      json_parse_error(js, "Unterminated object");
	    if (k->type != JSON_STRING)
	      json_parse_error(js, "Object key must be a string");

	    t = json_next_token(js);
	    if (t->type != JSON_NAME_SEP)
	      json_parse_error(js, "Colon expected");

	    struct json_node *v = json_next_value(js);
	    if (!v)
	      json_parse_error(js, "Unterminated object");
	    if (json_object_get(o, k->string))		// FIXME: Optimize
	      json_parse_error(js, "Key already set");
	    json_object_set(o, k->string, v);

	    t = json_next_token(js);
	    if (t->type == JSON_END_OBJECT)
	      break;
	    if (t->type != JSON_VALUE_SEP)
	      json_parse_error(js, "Comma expected");
	  }
	return o;
      }

    // Misplaced characters
    case JSON_END_ARRAY:
      json_parse_error(js, "Misplaced end of array");
    case JSON_END_OBJECT:
      json_parse_error(js, "Misplaced end of object");
    case JSON_NAME_SEP:
      json_parse_error(js, "Misplaced colon");
    case JSON_VALUE_SEP:
      json_parse_error(js, "Misplaced comma");
    default:
      ASSERT(0);
    }
}

struct json_node *json_parse(struct json_context *js, struct fastbuf *fb)
{
  json_set_input(js, fb);

  struct json_node *n = json_next_value(js);
  if (!n)
    json_parse_error(js, "Empty input");

  struct json_node *t = json_next_token(js);
  if (t->type != JSON_EOF)
    json_parse_error(js, "Only one top-level value allowed");

  return n;
}
