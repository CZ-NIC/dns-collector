/*
 *      Knuth-Morris-Pratt's Substring Search for N given strings
 *
 *      (c) 1999--2005, Robert Spalek <robert@ucw.cz>
 *      (c) 2006, Pavel Charvat <pchar@ucw.cz>
 *
 *      (In fact, the algorithm is usually referred to as Aho-McCorasick,
 *      but that's just an extension of KMP to multiple strings.)
 */

/*
 *  This is not a normal header file, it's a generator of KMP algorithm.
 *  Each time you include it with parameters set in the corresponding
 *  preprocessor macros, it generates KMP structures and functions
 *  with the parameters given.
 *
 *  This file contains only construction of the automaton. The search
 *  itself can be generated by inclusion of file lib/kmp-search.h.
 *  Separeted headers allow the user to define multiple search
 *  routines for one common set of key strings.
 *
 *  Example:
 *
 *	#define KMP_PREFIX(x) kmp_##x
 *	#define KMP_WANT_CLEANUP
 *	#define KMP_WANT_SEARCH // includes lib/kmp-search.h automatically
 *	#define KMPS_FOUND(kmp,src,s) printf("found\n")
 *	#include "lib/kmp.h"
 *
 *    [...]
 *
 *	struct kmp_struct kmp;	// a structure describing the whole automaton
 *	kmp_init(&kmp);		// initialization (must be called before all other functions)
 *
 *	// add key strings we want to search
 *	kmp_add(&kmp, "aaa");
 *	kmp_add(&kmp, "abc");
 *
 *	// complete the automaton, no more strings can be added later
 *	kmp_build(&kmp);
 *
 *	// example of search, should print single "found" to stdout
 *	kmp_run(&kmp, "aabaabca");
 *
 *	// destroy all internal structures
 *	kmp_cleanup(&kmp);
 *
 *
 *  Brief description of all parameters:
 *
 *    Basic parameters:
 *     	KMP_PREFIX(x)		macro to add a name prefix (used on all global names
 *				defined by the KMP generator); mandatory;
 *				we abbreviate this to P(x) below
 *
 *	KMP_CHAR		alphabet type, the default is u16
 *	
 *	KMP_SOURCE		user-defined text source; KMP_GET_CHAR must 
 *	KMP_GET_CHAR(kmp,src,c)	return zero at the end or nonzero together with the next character in c otherwise;
 *				if not defined, zero-terminated array of bytes is used as the input
 *	
 *	KMP_VARS		user-defined variables in 'struct P(struct)'
 *				-- a structure describing the whole automaton;
 *				these variables are stored in .u substructure to avoid collisions
 *	KMP_STATE_VARS		user-defined variables in 'struct P(state)'
 *				-- created for each state of the automaton;
 *				these variables are stored in .u substructure to avoid collisions
 *
 *    Parameters which select how the input is interpreted (if KMP_SOURCE is unset):
 *	KMP_USE_ASCII		reads single bytes from the input (default)
 *	KMP_USE_UTF8		reads UTF-8 characters from the input (valid UTF-8 needed)
 *	KMP_TOLOWER		converts all to lowercase
 *	KMP_UNACCENT		removes accents
 *	KMP_ONLYALPHA		converts non-alphas to KMP_CONTROL_CHAR (see below)
 *
 *    Parameters controlling add(kmp, src):
 * 	KMP_ADD_EXTRA_ARGS	extra arguments, should be used carefully because of possible collisions
 * 	KMP_ADD_INIT(kmp,src)	called in the beginning of add(), src is the first
 *      KMP_INIT_STATE(kmp,s)   initialization of a new state s (called before KMP_ADD_{NEW,DUP});
 *      			null state is not included and should be handled after init() if necessary;
 *      			all user-defined data are filled by zeros before call to KMP_INIT_STATE
 * 	KMP_ADD_NEW(kmp,src,s)	initialize last state of every new key string (called after KMP_INIT_STATE);
 * 				the string must be parsed before so src is after the last string's character
 * 	KMP_ADD_DUP(kmp,src,s)  analogy of KMP_ADD_NEW called for duplicates
 *
 *    Parameters to build():
 *      KMP_BUILD_STATE(kmp,s)	called for all states (including null) in order of non-decreasing tree depth
 *
 *    Other parameters:
 *	KMP_WANT_CLEANUP	define cleanup()
 *	KMP_WANT_SEARCH		includes lib/kmp-search.h with the same prefix;
 *				there can be multiple search variants for a single KMP automaton
 *	KMP_USE_POOL		allocates in a given pool
 *	KMP_CONTROL_CHAR	special control character (default is ':')
 *	KMP_GIVE_ALLOC		if set, you must supply custom allocation functions:
 *				void *alloc(unsigned int size) -- allocate space for
 *				a state. Default is pooled allocation from a local pool or HASH_USE_POOL.
 *				void free(void *) -- the converse.
 *	KMP_GIVE_HASHFN		if set, you must supply custom hash function:
 *				unsigned int hash(struct P(struct) *kmp, struct P(state) *state, KMP_CHAR c);
 *				default hash function works only for integer character types
 *	KMP_GIVE_EQ		if set, you must supply custom compare function of two characters:
 *				int eq(struct P(struct) *kmp, KMP_CHAR a, KMP_CHAR b);
 *				default is 'a == b'
 */

#ifndef KMP_PREFIX
#error Missing KMP_PREFIX
#endif

#include "lib/mempool.h"
#include <alloca.h>
#include <string.h>

#define P(x) KMP_PREFIX(x)

#ifdef KMP_CHAR
typedef KMP_CHAR P(char_t);
#else
typedef u16 P(char_t);
#endif

typedef u32 P(len_t);

#ifdef KMP_NODE
typedef KMP_NODE P(node_t);
#else
typedef struct {} P(node_t);
#endif

struct P(struct);

struct P(state) {
  struct P(state) *from;	/* state with the previous character (forms a tree with null state in the root) */
  struct P(state) *back;	/* backwards edge to the longest shorter state with same suffix */
  struct P(state) *next;	/* the longest of shorter matches (or NULL) */
  P(len_t) len;			/* state depth if it represents a key string, zero otherwise */
  P(char_t) c;			/* last character of the represented string */
  struct {
#   ifdef KMP_STATE_VARS
    KMP_STATE_VARS
#   endif    
  } u;				/* user-defined data*/
};

/* Control char */
static inline P(char_t)
P(control) (void)
{
# ifdef KMP_CONTROL_CHAR
  return KMP_CONTROL_CHAR;
# else
  return ':';
# endif
}

/* User-defined source */
struct P(hash_table);

#define HASH_GIVE_HASHFN
#ifdef KMP_GIVE_HASHFN
static inline uns
P(hash_hash) (struct P(hash_table) *t, struct P(state) *f, P(char_t) c)
{
  return P(hash) ((struct P(struct) *) t, f, c);
}
#else
static inline uns
P(hash_hash) (struct P(hash_table) *t UNUSED, struct P(state) *f, P(char_t) c)
{
  return (((uns)c) << 16) + (uns)(addr_int_t)f;
}
#endif

#ifndef KMP_GIVE_EQ
static inline int
P(eq) (struct P(struct) *kmp UNUSED, P(char_t) c1, P(char_t) c2)
{
  return c1 == c2;
}
#endif

static inline int
P(is_control) (struct P(struct) *kmp, P(char_t) c)
{
  return P(eq) (kmp, c, P(control)());
}

#define HASH_GIVE_EQ
static inline int
P(hash_eq) (struct P(hash_table) *t, struct P(state) *f1, P(char_t) c1, struct P(state) *f2, P(char_t) c2)
{
  return f1 == f2 && P(eq)((struct P(struct) *) t, c1, c2);
}

#ifdef KMP_GIVE_ALLOC
#define HASH_GIVE_ALLOC
static inline void *
P(hash_alloc) (struct P(hash_table) *t, uns size)
{
  return P(alloc) ((struct P(struct) *) t, size);
}

static inline void
P(hash_free) (struct P(hash_table) *t, void *ptr)
{
  P(free) ((struct P(struct) *) t, ptr);
}
#endif

#define HASH_GIVE_INIT_KEY
static inline void
P(hash_init_key) (struct P(hash_table) *t UNUSED, struct P(state) *s, struct P(state) *f, P(char_t) c)
{
  bzero(s, sizeof(*s));
# ifdef KMP_INIT_STATE  
  struct P(struct) *kmp = (struct P(struct) *)t;
  { KMP_INIT_STATE(kmp, s); }
# endif  
  s->from = f;
  s->c = c;
  s->next = f->back; /* the pointers hold the link-list of sons... changed in build() */
  f->back = s;
}

#undef P
#define HASH_PREFIX(x) KMP_PREFIX(hash_##x)
#define HASH_NODE struct KMP_PREFIX(state)
#define HASH_KEY_COMPLEX(x) x from, x c
#define HASH_KEY_DECL struct KMP_PREFIX(state) *from, KMP_PREFIX(char_t) c
#define HASH_WANT_NEW
#define HASH_WANT_FIND
#ifdef KMP_WANT_CLEANUP
#define HASH_WANT_CLEANUP
#endif
#if defined(KMP_USE_POOL)
#define HASH_USE_POOL KMP_USE_POOL
#else
#define HASH_AUTO_POOL 4096
#endif
#define HASH_CONSERVE_SPACE
#define HASH_TABLE_DYNAMIC
#include "lib/hashtable.h"
#define P(x) KMP_PREFIX(x)

struct P(struct) {
  struct P(hash_table) hash;		/* hash table of state transitions */
  struct P(state) null;			/* null state */
  struct {
#   ifdef KMP_VARS
    KMP_VARS
#   endif
  } u;					/* user-defined data */
};

#ifdef KMP_SOURCE
typedef KMP_SOURCE P(source_t);
#else
typedef byte *P(source_t);
#endif

#ifdef KMP_GET_CHAR
static inline int
P(get_char) (struct P(struct) *kmp UNUSED, P(source_t) *src UNUSED, P(char_t) *c UNUSED)
{
  return KMP_GET_CHAR(kmp, (*src), (*c));
}
#else
#  if defined(KMP_USE_UTF8)
#    include "lib/unicode.h"
#    if defined(KMP_ONLYALPHA) || defined(KMP_TOLOWER) || defined(KMP_UNACCENT)
#      include "charset/unicat.h"
#    endif
#  elif defined(KMP_USE_ASCII)
#    if defined(KMP_ONLYALPHA) || defined(KMP_TOLOWER)
#      include "lib/chartype.h"
#    endif
#  endif
static inline int
P(get_char) (struct P(struct) *kmp UNUSED, P(source_t) *src, P(char_t) *c)
{
# ifdef KMP_USE_UTF8
  uns cc;
  *src = (byte *)utf8_get(*src, &cc);
# ifdef KMP_ONLYALPHA
  if (!cc) {}
  else if (!Ualpha(cc))
    cc = P(control)();
  else
# endif  
    {
#     ifdef KMP_TOLOWER
      cc = Utolower(cc);
#     endif
#     ifdef KMP_UNACCENT
      cc = Uunaccent(cc);
#     endif
    }
# else
  uns cc = *(*src)++;
# ifdef KMP_ONLYALPHA
  if (!cc) {}
  else if (!Calpha(cc))
    cc = P(control)();
  else
# endif
#   ifdef KMP_TOLOWER
    cc = Clocase(cc);
#   endif
#   ifdef KMP_UNACCENT
#   error Do not know how to unaccent ASCII characters
#   endif
# endif
  *c = cc;
  return !!cc;
}
#endif

static struct P(state) *
P(add) (struct P(struct) *kmp, P(source_t) src
#   ifdef KMP_ADD_EXTRA_ARGS
    , KMP_ADD_EXTRA_ARGS
#   endif
)
{
# ifdef KMP_ADD_INIT
  { KMP_ADD_INIT(kmp, src); }
# endif

  P(char_t) c;
  if (!P(get_char)(kmp, &src, &c))
    return NULL;
  struct P(state) *p = &kmp->null, *s;
  uns len = 0;
  do
    {
      s = P(hash_find)(&kmp->hash, p, c);
      if (!s)
	for (;;)
	  {
	    s = P(hash_new)(&kmp->hash, p, c);
	    len++;
	    if (!(P(get_char)(kmp, &src, &c)))
	      goto enter_new;
	    p = s;
	  }
      p = s;
      len++;
    }
  while (P(get_char)(kmp, &src, &c));
  if (s->len)
    {
#     ifdef KMP_ADD_DUP
      { KMP_ADD_DUP(kmp, src, s); }
#     endif
      return s;
    }
enter_new:
  s->len = len;
# ifdef KMP_ADD_NEW
  { KMP_ADD_NEW(kmp, src, s); }
# endif
  return s;
}

static void
P(init) (struct P(struct) *kmp)
{
  bzero(&kmp->null, sizeof(struct P(state)));
  P(hash_init)(&kmp->hash);
}

#ifdef KMP_WANT_CLEANUP
static inline void
P(cleanup) (struct P(struct) *kmp)
{
  P(hash_cleanup)(&kmp->hash);
}
#endif

static inline int
P(empty) (struct P(struct) *kmp)
{
  return !kmp->hash.hash_count;
}

static inline struct P(state) *
P(chain_start) (struct P(state) *s)
{
  return s->len ? s : s->next;
}

static void
P(build) (struct P(struct) *kmp)
{
  if (P(empty)(kmp))
    return;
  uns read = 0, write = 0;
  struct P(state) *fifo[kmp->hash.hash_count], *null = &kmp->null;
  for (struct P(state) *s = null->back; s; s = s->next)
    fifo[write++] = s;
  null->back = NULL;
# ifdef KMP_BUILD_STATE
  { KMP_BUILD_STATE(kmp, null); }
# endif  
  while (read != write)
    {
      struct P(state) *s = fifo[read++], *t;
      for (t = s->back; t; t = t->next)
	fifo[write++] = t;
      for (t = s->from->back; 1; t = t->back)
        {
	  if (!t)
	    {
	      s->back = null;
	      s->next = NULL;
	      break;
	    }
	  s->back = P(hash_find)(&kmp->hash, t, s->c);
	  if (s->back)
	    {
	      s->next = s->back->len ? s->back : s->back->next;
	      break;
	    }
	}
#     ifdef KMP_BUILD_STATE
      { KMP_BUILD_STATE(kmp, s); }
#     endif      
    }
}

#undef P
#undef KMP_CHAR
#undef KMP_SOURCE
#undef KMP_GET_CHAR
#undef KMP_VARS
#undef KMP_STATE_VARS
#undef KMP_CONTEXT
#undef KMP_USE_ASCII
#undef KMP_USE_UTF8
#undef KMP_TOLOWER
#undef KMP_UNACCENT
#undef KMP_ONLYALPHA
#undef KMP_CONTROL_CHAR
#undef KMP_ADD_EXTRA_ARGS
#undef KMP_ADD_INIT
#undef KMP_ADD_NEW
#undef KMP_ADD_DUP
#undef KMP_INIT_STATE
#undef KMP_BUILD_STATE
#undef KMP_USE_POOL
#undef KMP_GIVE_ALLOC
#undef KMP_GIVE_HASHFN
#undef KMP_GIVE_EQ

#ifdef KMP_WANT_SEARCH
#  undef KMP_WANT_SEARCH
#  define KMPS_PREFIX(x) KMP_PREFIX(x)
#  define KMPS_KMP_PREFIX(x) KMP_PREFIX(x)
#  include "lib/kmp-search.h"
#endif

#undef KMP_PREFIX
