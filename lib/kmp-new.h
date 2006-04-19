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
 *
 *  [*]	KMP_PREFIX(x)		macro to add a name prefix (used on all global names
 *				defined by the KMP generator).
 *
 *	KMP_CHAR		alphabet type, the default is u16
 *	
 *	KMP_SOURCE		user-defined source; KMP_GET_CHAR must 
 *				return next character from the input or zero at the end;
 *				if not defined, zero-terminated array of bytes is used as the input
 *	KMP_GET_CHAR(ctx,src,c)
 *	
 *	KMP_NODE		user-defined data in each state
 *	KMP_CONTEXT		user-defined data in context
 *
 *    Parameters to default get_char():
 *	KMP_USE_ASCII		reads single bytes from the input (default)
 *	KMP_USE_UTF8		reads UTF-8 characters from the input (valid UTF-8 needed)
 *	KMP_TOLOWER		converts all to lowercase
 *	KMP_UNACCENT		removes accents
 *	KMP_ONLYALPHA		converts nonalphas to KMP_CONTROL_CHAR
 *	KMP_CONTROL_CHAR	special control character (default is ':')
 *
 *    Parameters to add():
 * 	KMP_ADD_EXTRA_ARGS	extra arguments
 * 	KMP_ADD_EXTRA_VAR	structure with extra local varriables
 * 	KMP_ADD_INIT(ctx,src,v)
 * 	KMP_ADD_NEW(ctx,src,v,s)
 * 	KMP_ADD_DUP(ctx,src,v,s)
 * 	KMP_NO_DUPS		no support for duplicates
 *
 *    Parameters to build():
 *      KMP_BUILD_STATE(ctx,s)	called for all states (including null) in order of non-decreasing tree depth
 *
 *	KMP_WANT_CLEANUP	cleanup()
 *	KMP_WANT_SEARCH		includes lib/kmp-search.h with the same prefix;
 *				there can be multiple search variants for a single KMP structure
 *
 *	KMP_USE_POOL		allocates on a given pool
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

struct P(state) {
  struct P(state) *from;	/* state with previous character */
  struct P(state) *back;	/* backwards edge to the largest shorter state */
  struct P(state) *next;	/* largest shorter match */
  P(len_t) len;			/* largest match, zero otherwise */
  P(char_t) c;			/* last character */
  P(node_t) n;			/* user-defined data */
};

/* Control char */
static inline P(char_t)
P(control_char) (void)
{
#ifdef KMP_CONTROL_CHAR
  return KMP_CONTROL_CHAR;
#else
  return ':';
#endif
}

/* User-defined source */
struct P(hash_table);

static inline uns
P(hash_hash) (struct P(hash_table) *t UNUSED, struct P(state) *f, P(char_t) c)
{
  return (((uns)c) << 16) + (uns)(addr_int_t)f;
}

static inline int
P(hash_eq) (struct P(hash_table) *t UNUSED, struct P(state) *f1, P(char_t) c1, struct P(state) *f2, P(char_t) c2)
{
  return f1 == f2 && c1 == c2;
}

static inline void
P(hash_init_key) (struct P(hash_table) *t UNUSED, struct P(state) *s, struct P(state) *f, P(char_t) c)
{
  bzero(s, sizeof(*s));
  s->from = f;
  s->c = c;
  s->next = f->back; /* the pointers hold the link-list of sons... change in build() */
  f->back = s;
}

#undef P
#define HASH_PREFIX(x) KMP_PREFIX(GLUE(hash_,x))
#define HASH_NODE struct KMP_PREFIX(state)
#define HASH_KEY_COMPLEX(x) x from, x c
#define HASH_KEY_DECL struct KMP_PREFIX(state) *from, KMP_PREFIX(char_t) c
#define HASH_WANT_NEW
#define HASH_WANT_FIND
#ifdef KMP_WANT_CLEANUP
#define HASH_WANT_CLEANUP
#endif
#define HASH_GIVE_HASHFN
#define HASH_GIVE_EQ
#define HASH_GIVE_INIT_KEY
#ifdef KMP_USE_POOL
#define HASH_USE_POOL KMP_USE_POOL
#else
#define HASH_AUTO_POOL 4096
#endif
#define HASH_CONSERVE_SPACE
#define HASH_TABLE_DYNAMIC
#include "lib/hashtable.h"
#define P(x) KMP_PREFIX(x)

struct P(context) {
  struct P(hash_table) hash;		/* hash table*/
  struct P(state) null;			/* null state */
# ifdef KMP_CONTEXT
  KMP_CONTEXT v;			/* user defined data */
# endif  
};

#ifdef KMP_SOURCE
typedef KMP_SOURCE P(source_t);
#else
typedef byte *P(source_t);
#endif

#ifdef KMP_GET_CHAR
static inline int
P(get_char) (struct P(context) *ctx UNUSED, P(source_t) *src UNUSED, P(char_t) *c UNUSED)
{
  return KMP_GET_CHAR(ctx, (*src), (*c));
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
P(get_char) (struct P(context) *ctx UNUSED, P(source_t) *src, P(char_t) *c)
{
# ifdef KMP_USE_UTF8
  uns cc;
  *src = (byte *)utf8_get(*src, &cc);
# ifdef KMP_ONLYALPHA
  if (!cc) {}
  else if (!Ualpha(cc))
    cc = P(control_char)();
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
    cc = P(control_char)();
  else
# endif
#   ifdef KMP_TOLOWER
    cc = Clocase(c);
#   endif
# endif
  *c = cc;
  return !!cc;
}
#endif

static struct P(state) *
P(add) (struct P(context) *ctx, P(source_t) src
#   ifdef KMP_ADD_EXTRA_ARGS
    , KMP_ADD_EXTRA_ARGS
#   endif
)
{
# ifdef KMP_ADD_EXTRA_VAR
  KMP_ADD_EXTRA_VAR v;
# endif
# ifdef KMP_ADD_INIT
  { KMP_ADD_INIT(ctx, src, v); }
# endif

  P(char_t) c;
  if (!P(get_char)(ctx, &src, &c))
    return NULL;
  struct P(state) *p = &ctx->null, *s;
  uns len = 0;
  do
    {
      s = P(hash_find)(&ctx->hash, p, c);
      if (!s)
	for (;;)
	  {
	    s = P(hash_new)(&ctx->hash, p, c);
	    len++;
	    if (!(P(get_char)(ctx, &src, &c)))
	      goto enter_new;
	    p = s;
	  }
      p = s;
      len++;
    }
  while (P(get_char)(ctx, &src, &c));
# ifdef KMP_NO_DUPS
  ASSERT(!s->len);
# else  
  if (s->len)
    {
#     ifdef KMP_ADD_DUP
      { KMP_ADD_DUP(ctx, src, v, s); }
#     endif
      return s;
    }
# endif  
enter_new:
  s->len = len;
# ifdef KMP_ADD_NEW
  { KMP_ADD_NEW(ctx, src, v, s); }
# endif
  return s;
}

static void
P(init) (struct P(context) *ctx)
{
  bzero(ctx, sizeof(*ctx));
  P(hash_init)(&ctx->hash);
}

#ifdef KMP_WANT_CLEANUP
static inline void
P(cleanup) (struct P(context) *ctx)
{
  P(hash_cleanup)(&ctx->hash);
}
#endif

static inline int
P(empty) (struct P(context) *ctx)
{
  return !ctx->hash.hash_count;
}

static inline struct P(state) *
P(chain_start) (struct P(state) *s)
{
  return s->len ? s : s->next;
}

static void
P(build) (struct P(context) *ctx)
{
  if (P(empty)(ctx))
    return;
  uns read = 0, write = 0;
  struct P(state) *fifo[ctx->hash.hash_count], *null = &ctx->null;
  for (struct P(state) *s = null->back; s; s = s->next)
    fifo[write++] = s;
  null->back = NULL;
# ifdef KMP_BUILD_STATE
  { KMP_BUILD_STATE(ctx, null); }
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
	  s->back = P(hash_find)(&ctx->hash, t, s->c);
	  if (s->back)
	    {
	      s->next = s->back->len ? s->back : s->back->next;
	      break;
	    }
	}
#     ifdef KMP_BUILD_STATE
      { KMP_BUILD_STATE(ctx, s); }
#     endif      
    }
}

#undef P
#undef KMP_CHAR
#undef KMP_SOURCE
#undef KMP_GET_CHAR
#undef KMP_NODE
#undef KMP_CONTEXT
#undef KMP_USE_ASCII
#undef KMP_USE_UTF8
#undef KMP_TOLOWER
#undef KMP_UNACCENT
#undef KMP_ONLYALPHA
#undef KMP_CONTROL_CHAR
#undef KMP_ADD_EXTRA_ARGS
#undef KMP_ADD_EXTRA_VAR
#undef KMP_ADD_INIT
#undef KMP_ADD_NEW
#undef KMP_ADD_DUP
#undef KMP_NO_DUPS
#undef KMP_BUILD_STATE
#undef KMP_USE_POOL

#ifdef KMP_WANT_SEARCH
#  undef KMP_WANT_SEARCH
#  define KMPS_PREFIX(x) KMP_PREFIX(x)
#  define KMPS_KMP_PREFIX(x) KMP_PREFIX(x)
#  include "lib/kmp-search.h"
#endif

#undef KMP_PREFIX
