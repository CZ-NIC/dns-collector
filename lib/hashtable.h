/*
 *	Sherlock Library -- Universal Hash Table
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

/*
 *  This is not a normal header file, it's a generator of hash tables.
 *  Each time you include it with parameters set in the corresponding
 *  preprocessor macros, it generates a hash table with the parameters
 *  given.
 *
 *  You need to specify:
 *
 *  HASH_NODE		data type where a node dwells (usually a struct).
 *  HASH_PREFIX(x)	macro to add a name prefix (used on all global names
 *			defined by the hash table generator).
 *
 *  Then decide on type of keys:
 *
 *  HASH_KEY_ATOMIC=f	use node->f as a key of an atomic type (i.e.,
 *			a type which can be compared using `==')
 *			HASH_ATOMIC_TYPE (defaults to int).
 *  | HASH_KEY_STRING=f	use node->f as a string key, allocated
 *			separately from the rest of the node.
 *  | HASH_KEY_ENDSTRING=f use node->f as a string key, allocated
 *			automatically at the end of the node struct
 *			(to be declared as "char f[1]" at the end).
 *  | HASH_KEY_COMPLEX	use a multi-component key; as the name suggests,
 *			the passing of parameters is a bit complex then.
 *			The HASH_KEY_COMPLEX(x) macro should expand to
 *			`x k1, x k2, ... x kn' and you should also define:
 *    HASH_KEY_DECL	declaration of function parameters in which key
 *			should be passed to all hash table operations.
 *			That is, `type1 k1, type2 k2, ... typen kn'.
 *			With complex keys, HASH_GIVE_HASHFN and HASH_GIVE_EQ
 *			are mandatory.
 *
 *  Then specify what operations you request (all names are automatically
 *  prefixed by calling HASH_PREFIX):
 *
 *  <always defined>	init() -- initialize the hash table.
 *  HASH_WANT_CLEANUP	cleanup() -- deallocate the hash table.
 *  HASH_WANT_FIND	node *find(key) -- find node with the specified
 *			key, return NULL if no such node exists.
 *  HASH_WANT_NEW	node *new(key) -- create new node with given key.
 *			Doesn't check whether it already exists.
 *  HASH_WANT_LOOKUP	node *lookup(key) -- find node with given key,
 *			if it doesn't exist, create it. Defining
 *			HASH_GIVE_INIT_DATA is strongly recommended.
 *  HASH_WANT_DELETE	int delete(key) -- delete and deallocate node
 *			with given key. Returns success.
 *  HASH_WANT_REMOVE	remove(node *) -- delete and deallocate given node.
 *
 *  You can also supply several functions:
 *
 *  HASH_GIVE_HASHFN	unsigned int hash(key) -- calculate hash value of key.
 *			We have sensible default hash functions for strings
 *			and integers.
 *  HASH_GIVE_EQ	int eq(key1, key2) -- return whether keys are equal.
 *			By default, we use == for atomic types and either
 *			strcmp or strcasecmp for strings.
 *  HASH_GIVE_EXTRA_SIZE int extra_size(key) -- returns how many bytes after the
 *			node should be allocated for dynamic data. Default=0
 *			or length of the string with HASH_KEY_ENDSTRING.
 *  HASH_GIVE_INIT_KEY	void init_key(node *,key) -- initialize key in a newly
 *			created node. Defaults: assignment for atomic keys
 *			and static strings, strcpy for end-allocated strings.
 *  HASH_GIVE_INIT_DATA	void init_data(node *) -- initialize data fields in a
 *			newly created node. Very useful for lookup operations.
 *  HASH_GIVE_ALLOC	void *alloc(unsigned int size) -- allocate space for
 *			a node. Default is either normal or pooled allocation
 *			depending on whether we want deletions.
 *			void free(void *) -- the converse.
 *
 *  ... and a couple of extra parameters:
 *
 *  HASH_NOCASE		string comparisons should be case-insensitive.
 *  HASH_DEFAULT_SIZE=n	initially, use hash table of `n' entries.
 *			The `n' has to be a power of two.
 *  HASH_CONSERVE_SPACE	use as little space as possible.
 *  HASH_FN_BITS=n	The hash function gives only `n' significant bits.
 *  HASH_ATOMIC_TYPE=t	Atomic values are of type `t' instead of int.
 *  HASH_USE_POOL=pool	Allocate all nodes from given mempool.
 *			Collides with delete/remove functions.
 *
 *  You also get a iterator macro at no extra charge:
 *
 *  HASH_FOR_ALL(hash_prefix, variable)
 *    {
 *      // node *variable gets declared automatically
 *      do_something_with_node(variable);
 *      // use HASH_BREAK and HASH_CONTINUE instead of break and continue
 *	// you must not alter contents of the hash table here
 *    }
 *  HASH_END_FOR;
 *
 *  Then include <lib/hashtable.h> and voila, you have a hash table
 *  suiting all your needs (at least those which you've revealed :) ).
 *
 *  After including this file, all parameter macros are automatically
 *  undef'd.
 */

#ifndef _SHERLOCK_HASHFUNC_H
#include "lib/hashfunc.h"
#endif

#include <string.h>

#if !defined(HASH_NODE) || !defined(HASH_PREFIX)
#error Some of the mandatory configuration macros are missing.
#endif

#define P(x) HASH_PREFIX(x)

/* Declare buckets and the hash table */

typedef HASH_NODE P(node);

typedef struct P(bucket) {
  struct P(bucket) *next;
#ifndef HASH_CONSERVE_SPACE
  uns hash;
#endif
  P(node) n;
} P(bucket);

struct P(table) {
  uns hash_size;
  uns hash_count, hash_max, hash_min, hash_hard_max, hash_mask;
  P(bucket) **ht;
} P(table);

#define T P(table)

/* Preset parameters */

#if defined(HASH_KEY_ATOMIC)

#define HASH_KEY(x) x HASH_KEY_ATOMIC

#ifndef HASH_ATOMIC_TYPE
#  define HASH_ATOMIC_TYPE int
#endif
#define HASH_KEY_DECL HASH_ATOMIC_TYPE HASH_KEY( )

#ifndef HASH_GIVE_HASHFN
#  define HASH_GIVE_HASHFN
   static inline int P(hash) (HASH_ATOMIC_TYPE x)
   { return hash_int(x); }
#endif

#ifndef HASH_GIVE_EQ
#  define HASH_GIVE_EQ
   static inline int P(eq) (HASH_ATOMIC_TYPE x, HASH_ATOMIC_TYPE y)
   { return x == y; }
#endif

#ifndef HASH_GIVE_INIT_KEY
#  define HASH_GIVE_INIT_KEY
   static inline void P(init_key) (P(node) *n, HASH_ATOMIC_TYPE k)
   { HASH_KEY(n->) = k; }
#endif

#ifndef HASH_CONSERVE_SPACE
#define HASH_CONSERVE_SPACE
#endif

#elif defined(HASH_KEY_STRING) || defined(HASH_KEY_ENDSTRING)

#ifdef HASH_KEY_STRING
#  define HASH_KEY(x) x HASH_KEY_STRING
#  ifndef HASH_GIVE_INIT_KEY
#    define HASH_GIVE_INIT_KEY
     static inline void P(init_key) (P(node) *n, char *k)
     { HASH_KEY(n->) = k; }
#  endif
#else
#  define HASH_KEY(x) x HASH_KEY_ENDSTRING
#  define HASH_GIVE_EXTRA_SIZE
   static inline int P(extra_size) (char *k)
   { return strlen(k); }
#  ifndef HASH_GIVE_INIT_KEY
#    define HASH_GIVE_INIT_KEY
     static inline void P(init_key) (P(node) *n, char *k)
     { strcpy(HASH_KEY(n->), k); }
#  endif
#endif
#define HASH_KEY_DECL char *HASH_KEY( )

#ifndef HASH_GIVE_HASHFN
#define HASH_GIVE_HASHFN
  static inline uns P(hash) (char *k)
   {
#    ifdef HASH_NOCASE
       return hash_string_nocase(k);
#    else
       return hash_string(k);
#    endif
   }
#endif

#ifndef HASH_GIVE_EQ
#  define HASH_GIVE_EQ
   static inline int P(eq) (char *x, char *y)
   {
#    ifdef HASH_NOCASE
       return !strcasecmp(x,y);
#    else
       return !strcmp(x,y);
#    endif
   }
#endif

#elif defined(HASH_KEY_COMPLEX)

#define HASH_KEY(x) HASH_KEY_COMPLEX(x)

#else
#error You forgot to set the hash key type.
#endif

/* Defaults for missing parameters */

#ifndef HASH_GIVE_HASHFN
#error Unable to determine which hash function to use.
#endif

#ifndef HASH_GIVE_EQ
#error Unable to determine how to compare two keys.
#endif

#ifdef HASH_GIVE_EXTRA_SIZE
/* This trickery is needed to avoid `unused parameter' warnings */
#define HASH_EXTRA_SIZE P(extra_size)
#else
/*
 *  Beware, C macros are expanded iteratively, not recursively,
 *  hence we get only a _single_ argument, although the expansion
 *  of HASH_KEY contains commas.
 */
#define HASH_EXTRA_SIZE(x) 0
#endif

#ifndef HASH_GIVE_INIT_KEY
#error Unable to determine how to initialize keys.
#endif

#ifndef HASH_GIVE_INIT_DATA
static inline void P(init_data) (P(node) *n UNUSED)
{
}
#endif

#include <stdlib.h>

#ifndef HASH_GIVE_ALLOC
#ifdef HASH_USE_POOL

static inline void * P(alloc) (unsigned int size)
{ return mp_alloc_fast(HASH_USE_POOL, size); }

#else

static inline void * P(alloc) (unsigned int size)
{ return xmalloc(size); }

static inline void P(free) (void *x)
{ xfree(x); }

#endif
#endif

#ifndef HASH_DEFAULT_SIZE
#define HASH_DEFAULT_SIZE 32
#endif

#ifndef HASH_FN_BITS
#define HASH_FN_BITS 32
#endif

/* Now the operations */

static void P(alloc_table) (void)
{
  T.ht = xmalloc(sizeof(void *) * T.hash_size);
  bzero(T.ht, sizeof(void *) * T.hash_size);
  T.hash_max = T.hash_size * 2;
  if (T.hash_max > T.hash_hard_max)
    T.hash_max = T.hash_hard_max;
  T.hash_min = T.hash_size / 4;
  T.hash_mask = T.hash_size - 1;
}

static void P(init) (void)
{
  T.hash_count = 0;
  T.hash_size = HASH_DEFAULT_SIZE;
#if HASH_FN_BITS < 28
  T.hash_hard_max = 1 << HASH_FN_BITS;
#else
  T.hash_hard_max = 1 << 28;
#endif
  P(alloc_table)();
}

#ifdef HASH_WANT_CLEANUP
static void P(cleanup) (void)
{
#ifndef HASH_USE_POOL
  uns i;
  P(bucket) *b, *bb;

  for (i=0; i<T.hash_size; i++)
    for (b=T.ht[i]; b; b=bb)
      {
	bb = b->next;
	P(free)(b);
      }
#endif
  xfree(T.ht);
}
#endif

static inline uns P(bucket_hash) (P(bucket) *b)
{
#ifdef HASH_CONSERVE_SPACE
  return P(hash)(HASH_KEY(b->n.));
#else
  return b->hash;
#endif
}

static void P(rehash) (uns size)
{
  P(bucket) *b, *nb;
  P(bucket) **oldt = T.ht, **newt;
  uns oldsize = T.hash_size;
  uns i, h;

  // log(L_DEBUG, "Rehashing %d->%d at count %d", oldsize, size, T.hash_count);
  T.hash_size = size;
  P(alloc_table)();
  newt = T.ht;
  for (i=0; i<oldsize; i++)
    {
      b = oldt[i];
      while (b)
	{
	  nb = b->next;
	  h = P(bucket_hash)(b) & T.hash_mask;
	  b->next = newt[h];
	  newt[h] = b;
	  b = nb;
	}
    }
  xfree(oldt);
}

#ifdef HASH_WANT_FIND
static P(node) * P(find) (HASH_KEY_DECL)
{
  uns h0 = P(hash) (HASH_KEY( ));
  uns h = h0 & T.hash_mask;
  P(bucket) *b;

  for (b=T.ht[h]; b; b=b->next)
    {
      if (
#ifndef HASH_CONSERVE_SPACE
	  b->hash == h0 &&
#endif
	  P(eq)(HASH_KEY( ), HASH_KEY(b->n.)))
	return &b->n;
    }
  return NULL;
}
#endif

#ifdef HASH_WANT_NEW
static P(node) * P(new) (HASH_KEY_DECL)
{
  uns h0, h;
  P(bucket) *b;

  h0 = P(hash) (HASH_KEY( ));
  h = h0 & T.hash_mask;
  b = P(alloc) (sizeof(struct P(bucket)) + HASH_EXTRA_SIZE(HASH_KEY( )));
  b->next = T.ht[h];
  T.ht[h] = b;
#ifndef HASH_CONSERVE_SPACE
  b->hash = h0;
#endif
  P(init_key)(&b->n, HASH_KEY( ));
  P(init_data)(&b->n);
  if (T.hash_count++ >= T.hash_max)
    P(rehash)(2*T.hash_size);
  return &b->n;
}
#endif

#ifdef HASH_WANT_LOOKUP
static P(node) * P(lookup) (HASH_KEY_DECL)
{
  uns h0 = P(hash) (HASH_KEY( ));
  uns h = h0 & T.hash_mask;
  P(bucket) *b;

  for (b=T.ht[h]; b; b=b->next)
    {
      if (
#ifndef HASH_CONSERVE_SPACE
	  b->hash == h0 &&
#endif
	  P(eq)(HASH_KEY( ), HASH_KEY(b->n.)))
	return &b->n;
    }

  b = P(alloc) (sizeof(struct P(bucket)) + HASH_EXTRA_SIZE(HASH_KEY( )));
  b->next = T.ht[h];
  T.ht[h] = b;
#ifndef HASH_CONSERVE_SPACE
  b->hash = h0;
#endif
  P(init_key)(&b->n, HASH_KEY( ));
  P(init_data)(&b->n);
  if (T.hash_count++ >= T.hash_max)
    P(rehash)(2*T.hash_size);
  return &b->n;
}
#endif

#ifdef HASH_WANT_DELETE
static int P(delete) (HASH_KEY_DECL)
{
  uns h0 = P(hash) (HASH_KEY( ));
  uns h = h0 & T.hash_mask;
  P(bucket) *b, **bb;

  for (bb=&T.ht[h]; b=*bb; bb=&b->next)
    {
      if (
#ifndef HASH_CONSERVE_SPACE
	  b->hash == h0 &&
#endif
	  P(eq)(HASH_KEY( ), HASH_KEY(b->n.)))
	{
	  *bb = b->next;
	  P(free)(b);
	  if (--T.hash_count < T.hash_min)
	    P(rehash)(T.hash_size/2);
	  return 1;
	}
    }
  return 0;
}
#endif

#ifdef HASH_WANT_REMOVE
static void P(remove) (P(node) *n)
{
  P(bucket) *x = SKIP_BACK(struct P(bucket), n, n);
  uns h0 = P(bucket_hash)(x);
  uns h = h0 & T.hash_mask;
  P(bucket) *b, **bb;

  for (bb=&T.ht[h]; (b=*bb) && b != x; bb=&b->next)
    ;
  ASSERT(b);
  *bb = b->next;
  P(free)(b);
  if (--T.hash_count < T.hash_min)
    P(rehash)(T.hash_size/2);
}
#endif

/* And the iterator */

#ifndef HASH_FOR_ALL

#define HASH_FOR_ALL(h_px, h_var)							\
do {											\
  uns h_slot;										\
  struct HASH_GLUE(h_px,bucket) *h_buck;						\
  for (h_slot=0; h_slot < HASH_GLUE(h_px,table).hash_size; h_slot++)			\
    for (h_buck = HASH_GLUE(h_px,table).ht[h_slot]; h_buck; h_buck = h_buck->next)	\
      {											\
	HASH_GLUE(h_px,node) *h_var = &h_buck->n;
#define HASH_END_FOR } } while(0)
#define HASH_BREAK 
#define HASH_CONTINUE continue
#define HASH_GLUE(x,y) x##_##y

#endif

/* Finally, undefine all the parameters */

#undef P
#undef T

#undef HASH_ATOMIC_TYPE
#undef HASH_CONSERVE_SPACE
#undef HASH_DEFAULT_SIZE
#undef HASH_EXTRA_SIZE
#undef HASH_FN_BITS
#undef HASH_GIVE_ALLOC
#undef HASH_GIVE_EQ
#undef HASH_GIVE_EXTRA_SIZE
#undef HASH_GIVE_HASHFN
#undef HASH_GIVE_INIT_DATA
#undef HASH_GIVE_INIT_KEY
#undef HASH_KEY
#undef HASH_KEY_ATOMIC
#undef HASH_KEY_COMPLEX
#undef HASH_KEY_DECL
#undef HASH_KEY_ENDSTRING
#undef HASH_KEY_STRING
#undef HASH_NOCASE
#undef HASH_NODE
#undef HASH_PREFIX
#undef HASH_USE_POOL
#undef HASH_WANT_CLEANUP
#undef HASH_WANT_DELETE
#undef HASH_WANT_FIND
#undef HASH_WANT_LOOKUP
#undef HASH_WANT_NEW
#undef HASH_WANT_REMOVE
