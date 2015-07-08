/*
 *	UCW JSON Library
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_JSON_JSON_H
#define _UCW_JSON_JSON_H

#include <ucw/clists.h>
#include <ucw/slists.h>
#include <ucw/mempool.h>
#include <ucw/fastbuf.h>

#ifdef CONFIG_UCW_CLEAN_ABI
// FIXME
#endif

/***
 * === FIXME
 ***/

struct json_context {
  struct mempool *pool;
  struct mempool_state init_state;

  struct fastbuf *in_fb;
  uint in_line;
  uint in_column;
  bool in_eof;
  struct json_node *next_token;
  struct json_node *trivial_token;
  int next_char;

  struct fastbuf *out_fb;
  uint out_indent;
  uint format_options;		// Public
};

struct json_context *json_new(void);
void json_delete(struct json_context *js);
void json_reset(struct json_context *js);

enum json_node_type {
  JSON_INVALID,
  JSON_NULL,
  JSON_BOOLEAN,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
  // These are not real nodes, but raw tokens
  JSON_BEGIN_ARRAY,
  JSON_END_ARRAY,
  JSON_BEGIN_OBJECT,
  JSON_END_OBJECT,
  JSON_NAME_SEP,
  JSON_VALUE_SEP,
  JSON_EOF,
};

struct json_node {
  enum json_node_type type;
  union {
    bool boolean;
    double number;
    const char *string;
    struct json_node **elements;	// Growing array
    struct json_pair *pairs;		// Growing array
  };
};

struct json_pair {
  const char *key;
  struct json_node *value;
  // FIXME: Hash table
};

struct json_node *json_new_node(struct json_context *js, enum json_node_type type);

static inline struct json_node *json_new_null(struct json_context *js)
{
  return json_new_node(js, JSON_NULL);
}

static inline struct json_node *json_new_bool(struct json_context *js, bool value)
{
  struct json_node *n = json_new_node(js, JSON_BOOLEAN);
  n->boolean = value;
  return n;
}

static inline struct json_node *json_new_number(struct json_context *js, double value)
{
  struct json_node *n = json_new_node(js, JSON_NUMBER);
  n->number = value;
  return n;
}

static inline struct json_node *json_new_string_ref(struct json_context *js, const char *value)
{
  struct json_node *n = json_new_node(js, JSON_STRING);
  n->string = value;
  return n;
}

static inline struct json_node *json_new_string(struct json_context *js, const char *value)
{
  return json_new_string_ref(js, mp_strdup(js->pool, value));
}

struct json_node *json_new_array(struct json_context *js);
void json_array_append(struct json_node *array, struct json_node *elt);

struct json_node *json_new_object(struct json_context *js);
// FIXME: key must not be freed
void json_object_set(struct json_node *n, const char *key, struct json_node *value);
struct json_node *json_object_get(struct json_node *n, const char *key);

void json_set_input(struct json_context *js, struct fastbuf *in);
struct json_node *json_peek_token(struct json_context *js);
struct json_node *json_next_token(struct json_context *js);

struct json_node *json_next_value(struct json_context *js);

struct json_node *json_parse(struct json_context *js, struct fastbuf *fb);

void json_set_output(struct json_context *js, struct fastbuf *fb);
void json_write_value(struct json_context *js, struct json_node *n);
void json_write(struct json_context *js, struct fastbuf *fb, struct json_node *n);

enum json_format_option {
  JSON_FORMAT_ESCAPE_NONASCII = 1,
  JSON_FORMAT_INDENT = 2,
};

#endif
