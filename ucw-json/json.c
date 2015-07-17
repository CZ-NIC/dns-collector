/*
 *	UCW JSON Library -- Data Representation
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/gary.h>
#include <ucw/mempool.h>
#include <ucw-json/json.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>

static void json_init(struct json_context *js)
{
  mp_save(js->pool, &js->init_state);
  js->trivial_token = json_new_node(js, JSON_INVALID);
}

struct json_context *json_new(void)
{
  struct mempool *mp = mp_new(4096);
  struct json_context *js = mp_alloc_zero(mp, sizeof(*js));
  js->pool = mp;
  json_init(js);
  return js;
}

void json_delete(struct json_context *js)
{
  mp_delete(js->pool);
}

void json_reset(struct json_context *js)
{
  struct mempool *mp = js->pool;
  mp_restore(mp, &js->init_state);
  bzero(js, sizeof(*js));
  js->pool = mp;
  json_init(js);
}

void json_push(struct json_context *js)
{
  ASSERT(!js->next_token);
  mp_push(js->pool);
}

void json_pop(struct json_context *js)
{
  ASSERT(!js->next_token);
  mp_pop(js->pool);
}

struct json_node *json_new_node(struct json_context *js, enum json_node_type type)
{
  struct json_node *n = mp_alloc_fast(js->pool, sizeof(*n));
  n->type = type;
  return n;
}

struct json_node *json_new_number(struct json_context *js, double value)
{
  ASSERT(isfinite(value));
  struct json_node *n = json_new_node(js, JSON_NUMBER);
  n->number = value;
  return n;
}

#define JSON_NUM_TO(_type, _min, _max)					\
  bool json_number_to_##_type(struct json_node *num, _type *dest)	\
  {									\
    if (num->type == JSON_NUMBER &&					\
        num->number >= _min && num->number <= _max)			\
      {									\
        *dest = num->number;						\
        return 1;							\
      }									\
    return 0;								\
  }

JSON_NUM_TO(int, INT_MIN, INT_MAX)
JSON_NUM_TO(uint, 0, UINT_MAX)
JSON_NUM_TO(s64, INT64_MIN, INT64_MAX)
JSON_NUM_TO(u64, 0, UINT64_MAX)

struct json_node *json_new_array(struct json_context *js)
{
  struct json_node *n = json_new_node(js, JSON_ARRAY);
  GARY_INIT_SPACE_ALLOC(n->elements, 4, mp_get_allocator(js->pool));
  return n;
}

void json_array_append(struct json_node *array, struct json_node *elt)
{
  ASSERT(array->type == JSON_ARRAY);
  *GARY_PUSH(array->elements) = elt;
}

struct json_node *json_new_object(struct json_context *js)
{
  struct json_node *n = json_new_node(js, JSON_OBJECT);
  GARY_INIT_SPACE_ALLOC(n->pairs, 4, mp_get_allocator(js->pool));
  return n;
}

void json_object_set(struct json_node *n, const char *key, struct json_node *value)
{
  for (size_t i=0; i < GARY_SIZE(n->pairs); i++)
    if (!strcmp(n->pairs[i].key, key))
      {
	if (value)
	  n->pairs[i].value = value;
	else
	  {
	    n->pairs[i] = n->pairs[GARY_SIZE(n->pairs) - 1];
	    GARY_POP(n->pairs);
	  }
	return;
      }

  if (value)
    {
      struct json_pair *p = GARY_PUSH(n->pairs);
      p->key = key;
      p->value = value;
    }
}

struct json_node *json_object_get(struct json_node *n, const char *key)
{
  for (size_t i=0; i < GARY_SIZE(n->pairs); i++)
    if (!strcmp(n->pairs[i].key, key))
      return n->pairs[i].value;
  return NULL;
}
