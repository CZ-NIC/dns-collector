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
#define json_array_append ucw_json_array_append
#define json_delete ucw_json_delete
#define json_new ucw_json_new
#define json_new_array ucw_json_new_array
#define json_new_node ucw_json_new_node
#define json_new_object ucw_json_new_object
#define json_next_token ucw_json_next_token
#define json_next_value ucw_json_next_value
#define json_object_get ucw_json_object_get
#define json_object_set ucw_json_object_set
#define json_parse ucw_json_parse
#define json_peek_token ucw_json_peek_token
#define json_pop ucw_json_pop
#define json_push ucw_json_push
#define json_reset ucw_json_reset
#define json_set_input ucw_json_set_input
#define json_set_output ucw_json_set_output
#define json_write ucw_json_write
#define json_write_value ucw_json_write_value
#endif

/***
 * === JSON library context
 *
 * The context structure remembers the whole state of the JSON
 * library. All JSON values are allocated from a memory pool associated
 * with the context. By default, their lifetime is the same as that
 * of the context.
 *
 * Alternatively, you can mark the current state of the context
 * with json_push() and return to the marked state later using
 * json_pop(). All JSON values created between these two operations
 * are released afterwards. See json_push() for details.
 ***/

/**
 * The context is represented a pointer to this structure.
 * The fields marked with [*] are publicly accessible, the rest is private.
 **/
struct json_context {
  // Memory management
  struct mempool *pool;
  struct mempool_state init_state;

  // Parser context
  struct fastbuf *in_fb;
  uint in_line;				// [*] Current line number
  uint in_column;			// [*] Current column number
  bool in_eof;				//     End of file was encountered
  struct json_node *next_token;
  struct json_node *trivial_token;
  int next_char;

  // Formatter context
  struct fastbuf *out_fb;
  uint out_indent;
  uint format_options;			// [*] Formatting options (a combination of JSON_FORMAT_xxx)
};

/** Creates a new JSON context. **/
struct json_context *json_new(void);

/** Deletes a JSON context, deallocating all memory associated with it. **/
void json_delete(struct json_context *js);

/**
 * Recycles a JSON context. All state is reset, allocated objects are freed.
 * This is equivalent to mp_delete() followed by mp_new(), but it is faster
 * and the address of the context is preserved.
 **/
void json_reset(struct json_context *js);

/**
 * Push the current state of the context onto state stack.
 *
 * Between json_push() and the associated json_pop(), only newly
 * created JSON values can be modified. Older values can be only
 * inspected, never modified. In particular, new values cannot be
 * inserted to old arrays nor objects.
 *
 * If you are using json_peek_token(), the saved tokens cannot
 * be carried over push/pop boundary.
 **/
void json_push(struct json_context *js);

/**
 * Pop state of the context off state stack. All JSON values created
 * since the state was saved by json_push() are released.
 **/
void json_pop(struct json_context *js);

/***
 * === JSON values
 *
 * Each JSON value is represented by <<struct json_node,struct json_node>>,
 * which is either an elementary value (null, boolean, number, string),
 * or a container (array, object) pointing to other values.
 *
 * A value can belong to multiple containers simultaneously, so in general,
 * the relationships between values need not form a tree, but a directed
 * acyclic graph.
 *
 * You are allowed to read contents of nodes directly, but construction
 * and modification of nodes must be always performed using the appropriate
 * library functions.
 ***/

/** Node types **/
enum json_node_type {
  JSON_INVALID,
  JSON_NULL,
  JSON_BOOLEAN,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
  // These are not real nodes, but raw tokens.
  // They are not present in the tree of values, but you may see them
  // if you call json_next_token() and friends.
  JSON_BEGIN_ARRAY,
  JSON_END_ARRAY,
  JSON_BEGIN_OBJECT,
  JSON_END_OBJECT,
  JSON_NAME_SEP,
  JSON_VALUE_SEP,
  JSON_EOF,
};

/** Each value is represented by a single node. **/
struct json_node {
  enum json_node_type type;
  union {				// Data specific to individual value types
    bool boolean;
    double number;
    const char *string;
    struct json_node **elements;	// Arrays: Growing array of values
    struct json_pair *pairs;		// Objects: Growing array of pairs
  };
};

/** Attributes of objects are stored as (key, value) pairs of this format. **/
struct json_pair {
  const char *key;
  struct json_node *value;
  // FIXME: Hash table
};

// Used internally
struct json_node *json_new_node(struct json_context *js, enum json_node_type type);

/** Creates a new null value. **/
static inline struct json_node *json_new_null(struct json_context *js UNUSED)
{
  static const struct json_node static_null = { .type = JSON_NULL };
  return (struct json_node *) &static_null;
}

/** Creates a new boolean value. **/
static inline struct json_node *json_new_bool(struct json_context *js UNUSED, bool value)
{
  static const struct json_node static_bool[2] = {
    [0] = { .type = JSON_BOOLEAN, .boolean = 0 },
    [1] = { .type = JSON_BOOLEAN, .boolean = 1 },
  };
  return (struct json_node *) &static_bool[value];
}

/** Creates a new numeric value. **/
static inline struct json_node *json_new_number(struct json_context *js, double value)
{
  struct json_node *n = json_new_node(js, JSON_NUMBER);
  n->number = value;
  return n;
}

/** Creates a new string value. The @value is kept only as a reference. **/
static inline struct json_node *json_new_string_ref(struct json_context *js, const char *value)
{
  struct json_node *n = json_new_node(js, JSON_STRING);
  n->string = value;
  return n;
}

/** Creates a new string value, making a private copy of @value. **/
static inline struct json_node *json_new_string(struct json_context *js, const char *value)
{
  return json_new_string_ref(js, mp_strdup(js->pool, value));
}

/** Creates a new array value with no elements. **/
struct json_node *json_new_array(struct json_context *js);

/** Appends a new element to the given array. **/
void json_array_append(struct json_node *array, struct json_node *elt);

/** Creates a new object value with no attributes. **/
struct json_node *json_new_object(struct json_context *js);

/**
 * Adds a new (@key, @value) pair to the given object. If @key is already
 * present, the pair is replaced. If @value is NULL, no new pair is created
 * and a pre-existing pair is deleted.
 *
 * The @key is referenced by the object, you must not free it during
 * the lifetime of the JSON context.
 *
 * FIXME: Add json_copy_key().
 **/
void json_object_set(struct json_node *n, const char *key, struct json_node *value);

/** Returns the value associated with @key, or NULL if no such value exists. **/
struct json_node *json_object_get(struct json_node *n, const char *key);

/***
 * === Parser
 *
 * The simplest way to parse a complete JSON file is to call json_parse(),
 * which returns a value tree representing the contents of the file.
 *
 * Alternatively, you can read the input token by token: call json_set_input()
 * and then repeat json_next_token(). If you are parsing huge JSON files,
 * you probably want to do json_push() first, then scan and process some
 * tokens, and then json_pop().
 ***/

/** Parses a JSON file from the given fastbuf stream. **/
struct json_node *json_parse(struct json_context *js, struct fastbuf *fb);

/** Selects the given fastbuf stream as parser input. **/
void json_set_input(struct json_context *js, struct fastbuf *in);

/** Reads the next token from the input. **/
struct json_node *json_next_token(struct json_context *js);

/** Reads the next token, but keeps it in the input. **/
struct json_node *json_peek_token(struct json_context *js);

/** Reads the next JSON value, including nested values. **/
struct json_node *json_next_value(struct json_context *js);

/***
 * === Writer
 *
 * JSON files can be produced by simply calling json_write().
 *
 * If you want to generate the output on the fly (for example if it is huge),
 * call json_set_output() and then iterate json_write_value().
 *
 * By default, we produce a single-line compact representation,
 * but you can choose differently by setting the appropriate
 * `format_options` in the `json_context`.
 ***/

/** Writes a JSON file to the given fastbuf stream, containing the JSON value @n. **/
void json_write(struct json_context *js, struct fastbuf *fb, struct json_node *n);

/** Selects the given fastbuf stream as output. **/
void json_set_output(struct json_context *js, struct fastbuf *fb);

/** Writes a single JSON value to the output stream. **/
void json_write_value(struct json_context *js, struct json_node *n);

/** Formatting options. The `format_options` field in the context is a bitwise OR of these flags. **/
enum json_format_option {
  JSON_FORMAT_ESCAPE_NONASCII = 1,	// Produce pure ASCII output by escaping all Unicode characters in strings
  JSON_FORMAT_INDENT = 2,		// Produce pretty indented output
};

#endif
