/*
 *	UCW Library -- Byte-based trie
 *
 *	(c) 2008 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *	This is not a normal header file, it's a generator of tries.
 *	Each time you include it with parameters set in the corresponding
 *	preprocessor macros, it generates a trie with the parameters given.
 *
 *	You need to specify:
 *
 *	[*] TRIE_PREFIX(x)		macro to add a name prefix (used on all global names
 *					defined by the trie generator).
 *
 *	[*] TRIE_NODE_TYPE		data type where a node dwells (usually a struct, default=void).
 *	    TRIE_NODE_KEY(node)		macro to return the pointer to the key (default=&x)
 *	    TRIE_NODE_LEN(node)		macro to return the length of the key (default=str_len(TRIE_NODE_KEY(node)))
 *	    TRIE_LEN_TYPE		integer type large enough to hold length of any inserted string (default=u32).
 *	    TRIE_REV			work with reversed strings.
 *	    TRIE_DYNAMIC
 *
 *	    TRIE_WANT_CLEANUP		cleanup()
 *
 *	    TRIE_WANT_FIND		node *find(char *str)
 *	    TRIE_WANT_FIND_BUF		node *find_buf(byte *ptr, uns len)
 *	    TRIE_WANT_ADD		add(*node)
 *	    TRIE_WANT_REPLACE		node *replace(*node)
 * 	    TRIE_WANT_DELETE		delete(char *str)
 * 	    TRIE_WANT_DELETE_BUF	delete_buf(byte *ptr, uns len)
 * 	    TRIE_WANT_REMOVE		remove(*node)
 *
 *	    TRIE_WANT_WALK_ALL		walk_all(walk)
 *	    TRIE_WANT_WALK_PREFIX	walk_prefix(walk, char *str)
 *	    TRIE_WANT_AUDIT		audit()
 *	    TRIW_WANT_STATS
 */

/*** Define once ***/

#ifndef _SHERLOCK_UCW_TRIE_H
#define _SHERLOCK_UCW_TRIE_H

#include "lib/eltpool.h"
#include "lib/hashfunc.h"

#include <string.h>

#define TRIE_FLAG_DEG		0x01ff	// mask for edge degree (0-256)
#define TRIE_FLAG_HASH		0x0200	// sons are stored in a hash table
#define TRIE_FLAG_NODE		0x0400	// edge contains inserted data

#endif

/*** Defaults ***/

#ifndef TRIE_PREFIX
#error Undefined mandatory macro TRIE_PREFIX
#endif
#define P(x) TRIE_PREFIX(x)

#ifndef TRIE_NODE_TYPE
#error Undefined mandatory macro TRIE_NODE_TYPE
#endif
typedef TRIE_NODE_TYPE P(node_t);

#ifndef TRIE_NODE_KEY
#define TRIE_NODE_KEY(node) ((char *)&(node))
#endif

#ifndef TRIE_NODE_LEN
#define TRIE_NODE_LEN(node) (str_len(TRIE_NODE_KEY(node)))
#endif

#ifndef TRIE_LEN_TYPE
#define TRIE_LEN_TYPE u32
#endif
typedef TRIE_LEN_TYPE P(len_t);

#ifndef TRIE_ELTPOOL_SIZE
#define TRIE_ELTPOOL_SIZE 1024
#endif

#ifndef TRIE_HASH_THRESHOLD
#define TRIE_HASH_THRESHOLD (6 - sizeof(P(len_t)))
#endif

#ifndef TRIE_BUCKET_SIZE
#define TRIE_BUCKET_SIZE (sizeof(void *) - 1)
#endif

#define TRIE_COMPILE_ASSERT(x, y) typedef char TRIE_PREFIX(x##_compile_assert)[!!(y)-1]
TRIE_COMPILE_ASSERT(len_type, sizeof(P(len_t)) <= sizeof(uns));
TRIE_COMPILE_ASSERT(hash_threshold, TRIE_HASH_THRESHOLD >= 2);
TRIE_COMPILE_ASSERT(bucket_size, TRIE_BUCKET_SIZE >= 3 && TRIE_BUCKET_SIZE <= 255);

#ifdef TRIE_TRACE
#define TRIE_DBG(x...) msg(L_DEBUG, "TRIE: " x)
#else
#define TRIE_DBG(x...) do{}while(0)
#endif

/*** Solve dependencies ***/

#if !defined(TRIE_WANT_WALK_ALL) && defined(TRIE_WANT_AUDIT)
#define TRIE_WANT_WALK_ALL
#endif

#if !defined(TRIE_WANT_WALK_SUBTREE) && (defined(TRIE_WANT_WALK_ALL) || defined(TRIE_WANT_WALK_PREFIX))
#define TRIE_WANT_WALK_SUBTREE
#endif

#if !defined(TRIE_WANT_DO_FIND) && (defined(TRIE_WANT_FIND) || defined(TRIE_WANT_FIND_BUF))
#define TRIE_WANT_DO_FIND
#endif

#if !defined(TRIE_WANT_DO_FIND_PREFIX) && defined(TRIE_WANT_WALK_PREFIX)
#define TRIE_WANT_DO_FIND_PREFIX
#endif

#if !defined(TRIE_WANT_DO_LOOKUP) && (defined(TRIE_WANT_ADD) || defined(TRIE_WANT_REPLACE))
#define TRIE_WANT_DO_LOOKUP
#endif

#if !defined(TRIE_WANT_DO_DELETE) && (defined(TRIE_WANT_DELETE) || defined(TRIE_WANT_DELETE_BUF) || defined(TRIE_WANT_REMOVE))
#define TRIE_WANT_DO_DELETE
#endif

#if !defined(TRIE_WANT_DO_LOOKUP)
#error You must request at least one method for inserting nodes
#endif

/*** Data structures ***/

struct P(trie) {
  struct P(edge) *root;				// root edge or NULL
  struct eltpool *epool[TRIE_HASH_THRESHOLD + 1]; // eltpools for edges with array of sons
  struct eltpool *hpool[9];			// eltpools for edges with hash table
};

struct P(bucket) {				// hash bucket
  byte count;					// number of inserted + deleted entries
  byte trans[TRIE_BUCKET_SIZE];			// transition characters
  struct P(edge) *son[TRIE_BUCKET_SIZE];	// sons (or NULL for deleted entries)
};

struct P(edge) {
  u16 flags;					// TRIE_FLAG_x
  union {
    byte trans[TRIE_HASH_THRESHOLD];		// transition characters (!TRIE_FLAG_HASH)
    struct {
      byte hash_rank;				// logarithmic hash size (TRIE_FLAG_HASH)
      byte hash_deleted;			// number of deleted items
    };
  };
  P(len_t) len;					// sum of all ancestor edges with their trasition
						//   characters plus the length of the current edge
  union {
    P(node_t) *node;				// inserted data (TRIE_FLAG_NODE)
    struct P(edge) *leaf;			// reference to a descendant with data (!TRIE_FLAG_NODE)
  };
  union {
    struct P(bucket) hash[0];			// array of buckets (TRIE_FLAG_HASH)
    struct P(edge) *son[0];			// array of sons (!TRIE_FLAG_HASH)
  };
};

#ifdef TRIE_DYNAMIC
#define T (*trie)
#define TA struct P(trie) *trie
#define TAC TA,
#define TT trie
#define TTC trie,
#else
static struct P(trie) P(trie);
#define T P(trie)
#define TA void
#define TAC
#define TT
#define TTC
#endif

/*** Memory management ***/

static void
P(init)(TA)
{
  TRIE_DBG("Initializing");
  bzero(&T, sizeof(T));
  for (uns i = 0; i < ARRAY_SIZE(T.epool); i++)
    {
      uns size = sizeof(struct P(edge)) + i * sizeof(void *);
      T.epool[i] = ep_new(size, MAX(TRIE_ELTPOOL_SIZE / size, 1));
    }
  for (uns i = 0; i < ARRAY_SIZE(T.hpool); i++)
    {
      uns size = sizeof(struct P(edge)) + (sizeof(struct P(bucket)) << i);
      T.hpool[i] = ep_new(size, MAX(TRIE_ELTPOOL_SIZE / size, 1));
    }
}

#ifdef TRIE_WANT_CLEANUP
static void
P(cleanup)(TA)
{
  TRIE_DBG("Cleaning up");
  for (uns i = 0; i < ARRAY_SIZE(T.epool); i++)
    ep_delete(T.epool[i]);
  for (uns i = 0; i < ARRAY_SIZE(T.hpool); i++)
    ep_delete(T.hpool[i]);
}
#endif

static struct P(edge) *
P(edge_alloc)(TAC uns flags)
{
  struct P(edge) *edge;
  if (flags & TRIE_FLAG_HASH)
    {
      uns rank = 0, deg = flags & TRIE_FLAG_DEG;
      while ((TRIE_BUCKET_SIZE << rank) < deg * 2) // 25-50% density
	rank++;
      ASSERT(rank < ARRAY_SIZE(T.hpool));
      edge = ep_alloc(T.hpool[rank]);
      edge->hash_rank = rank;
      edge->hash_deleted = 0;
      bzero(edge->hash, sizeof(struct P(bucket)) << rank);
    }
  else
    edge = ep_alloc(T.epool[flags & TRIE_FLAG_DEG]);
  edge->flags = flags;
  TRIE_DBG("Allocated edge %p, flags=0x%x", edge, flags);
  return edge;
}

static void
P(edge_free)(TAC struct P(edge) *edge)
{
  TRIE_DBG("Freeing edge %p, flags=0x%x", edge, edge->flags);
  if (edge->flags & TRIE_FLAG_HASH)
    ep_free(T.hpool[edge->hash_rank], edge);
  else
    ep_free(T.epool[edge->flags & TRIE_FLAG_DEG], edge);
}

/*** Manipulation with strings ***/

static inline byte *
P(str_get)(P(node_t) *node)
{
  return TRIE_NODE_KEY((*node));
}

static inline uns
P(str_len)(P(node_t) *node)
{
  return TRIE_NODE_LEN((*node));
}

static inline uns
P(str_char)(byte *ptr, uns len UNUSED, uns pos)
{
#ifndef TRIE_REV
  return ptr[pos];
#else
  return ptr[len - pos - 1];
#endif
}

static inline byte *
P(str_prefix)(byte *ptr, uns len UNUSED, uns prefix UNUSED)
{
#ifndef TRIE_REV
  return ptr;
#else
  return ptr + len - prefix;
#endif
}

static inline byte *
P(str_suffix)(byte *ptr, uns len UNUSED, uns suffix UNUSED)
{
#ifndef TRIE_REV
  return ptr + len - suffix;
#else
  return ptr;
#endif
}

/*** Sons ***/

static inline uns
P(hash_func)(uns c)
{
  return hash_u32(c) >> 16;
}

static inline struct P(edge) **
P(hash_find)(struct P(edge) *edge, uns c)
{
  uns mask = (1U << edge->hash_rank) - 1;
  for (uns x = P(hash_func)(c); ; x++)
    {
      struct P(bucket) *b = &edge->hash[x & mask];
      for (uns i = 0; i < b->count; i++)
	if (b->trans[i] == c && b->son[i])
	  return &b->son[i];
      if (b->count != TRIE_BUCKET_SIZE)
	return NULL;
    }
}

static struct P(edge) **
P(hash_insert)(struct P(edge) *edge, uns c)
{
  uns mask = (1U << edge->hash_rank) - 1, i, x;
  struct P(bucket) *b;
  for (x = P(hash_func)(c); ; x++)
    {
      b = &edge->hash[x & mask];
      for (i = 0; i < b->count; i++)
	if (!b->son[i])
	  {
	    edge->hash_deleted--;
	    goto end;
	  }
      if (i != TRIE_BUCKET_SIZE)
        {
	  b->count = i + 1;
	  break;
	}
    }
end:
  b->trans[i] = c;
  return &b->son[i];
}

#ifdef TRIE_WANT_DO_DELETE
static void
P(hash_delete)(struct P(edge) *edge, uns c)
{
  uns mask = (1U << edge->hash_rank) - 1;
  for (uns x = P(hash_func)(c); ; x++)
    {
      struct P(bucket) *b = &edge->hash[x & mask];
      for (uns i = 0; i < b->count; i++)
	if (b->trans[i] == c && b->son[i])
	  {
	    b->trans[i] = 0; // not necessary
	    b->son[i] = NULL;
	    edge->hash_deleted++;
	    return;
	  }
      if (b->count != TRIE_BUCKET_SIZE)
	ASSERT(0);
    }
}
#endif

#define TRIE_HASH_FOR_ALL(xedge, xtrans, xson) do { \
  struct P(edge) *_edge = (xedge); \
  for (struct P(bucket) *_b = _edge->hash + (1U << _edge->hash_rank); --_b >= _edge->hash; ) \
    for (uns _i = 0; _i < _b->count; _i++) \
      if (_b->son[_i]) { \
	UNUSED uns xtrans = _b->trans[_i]; \
	UNUSED struct P(edge) *xson = _b->son[_i]; \
	do {
#define TRIE_HASH_END_FOR }while(0); } }while(0)

static void
P(hash_realloc)(TAC struct P(edge) **ref)
{
  struct P(edge) *old = *ref, *edge = *ref = P(edge_alloc)(TTC old->flags);
  TRIE_DBG("Reallocating hash table");
  edge->node = old->node;
  edge->len = old->len;
  TRIE_HASH_FOR_ALL(old, trans, son)
    *P(hash_insert)(edge, trans) = son;
  TRIE_HASH_END_FOR;
  P(edge_free)(TTC old);
}

/*** Finding/inserting/deleting sons ***/

static struct P(edge) **
P(son_find)(struct P(edge) *edge, uns c)
{
  if (edge->flags & TRIE_FLAG_HASH)
    return P(hash_find)(edge, c);
  else
    for (uns i = edge->flags & TRIE_FLAG_DEG; i--; )
      if (edge->trans[i] == c)
	return &edge->son[i];
  return NULL;
}

static struct P(edge) **
P(son_insert)(TAC struct P(edge) **ref, uns c)
{
  struct P(edge) *old = *ref, *edge;
  uns deg = old->flags & TRIE_FLAG_DEG;
  if (old->flags & TRIE_FLAG_HASH)
    {
      old->flags++;
      if ((deg + 1 + old->hash_deleted) * 4 > (TRIE_BUCKET_SIZE << old->hash_rank) * 3) // >75% density
        {
	  P(hash_realloc)(TTC ref);
	  edge = *ref;
	}
      else
	edge = old;
    }
  else
    {
      if (deg < TRIE_HASH_THRESHOLD)
        {
	  TRIE_DBG("Growing array");
	  edge = P(edge_alloc)(TTC old->flags + 1);
	  memcpy((byte *)edge + sizeof(edge->flags), (byte *)old + sizeof(edge->flags),
	    sizeof(*old) - sizeof(edge->flags) + deg * sizeof(*old->son));
	  edge->trans[deg] = c;
	  edge->son[deg] = NULL;
	  P(edge_free)(TTC old);
	  *ref = edge;
	  return &edge->son[deg];
	}
      else
        {
	  TRIE_DBG("Growing array to hash table");
	  edge = P(edge_alloc)(TTC (old->flags + 1) | TRIE_FLAG_HASH);
	  edge->node = old->node;
	  edge->len = old->len;
	  for (uns i = 0; i < deg; i++)
	    *P(hash_insert)(edge, old->trans[i]) = old->son[i];
	  P(edge_free)(TTC old);
	}
    }
  *ref = edge;
  return P(hash_insert)(edge, c);
}

#ifdef TRIE_WANT_DO_DELETE
static void
P(son_delete)(TAC struct P(edge) **ref, uns c)
{
  struct P(edge) *old = *ref, *edge;
  uns deg = old->flags & TRIE_FLAG_DEG;
  ASSERT(deg);
  if (old->flags & TRIE_FLAG_HASH)
    {
      P(hash_delete)(old, c);
      old->flags--;
      deg--;
      if (deg <= TRIE_HASH_THRESHOLD / 2)
        {
	  TRIE_DBG("Reducing hash table to array");
	  edge = P(edge_alloc)(TTC old->flags & ~TRIE_FLAG_HASH);
	  uns k = 0;
	  TRIE_HASH_FOR_ALL(old, trans, son)
	    edge->trans[k] = trans;
	    edge->son[k] = son;
	    k++;
	  TRIE_HASH_END_FOR;
	  ASSERT(k == deg);
	}
      else if (deg * 6 >= (TRIE_BUCKET_SIZE << old->hash_rank)) // >= 16%
	return;
      else
        {
	  P(hash_realloc)(TTC ref);
	  edge = *ref;
	  return;
	}
    }
  else
    {
      TRIE_DBG("Reducing array");
      edge = P(edge_alloc)(TTC old->flags - 1);
      uns j = 0;
      for (uns i = 0; i < deg; i++)
	if (old->trans[i] != c)
	  {
	    edge->trans[j] = old->trans[i];
	    edge->son[j] = old->son[i];
	    j++;
	  }
      ASSERT(j == deg - 1);
    }
  edge->node = old->node;
  edge->len = old->len;
  P(edge_free)(TTC old);
  *ref = edge;
}
#endif

#ifdef TRIE_WANT_DO_DELETE
static struct P(edge) *
P(son_any)(struct P(edge) *edge)
{
  ASSERT(edge->flags & TRIE_FLAG_DEG);
  if (!(edge->flags & TRIE_FLAG_HASH))
    return edge->son[0];
  else
    for (struct P(bucket) *b = edge->hash; ; b++)
      for (uns i = 0; i < b->count; i++)
	if (b->son[i])
	  return b->son[i];
}
#endif

/*** Find/insert/delete ***/

#ifdef TRIE_WANT_DO_FIND
static struct P(edge) *
P(do_find)(TAC byte *ptr, uns len)
{
  TRIE_DBG("do_find('%.*s')", len, ptr);
  struct P(edge) **ref = &T.root, *edge;
  do
    {
      if (!(edge = *ref) || edge->len > len)
	return NULL;
      else if (edge->len == len)
	return ((edge->flags & TRIE_FLAG_NODE) && !memcmp(ptr, P(str_get)(edge->node), len)) ? edge : NULL;
    }
  while (ref = P(son_find)(edge, P(str_char)(ptr, len, edge->len)));
  return NULL;
}
#endif

#ifdef TRIE_WANT_DO_FIND_PREFIX
static struct P(edge) *
P(do_find_prefix)(TAC byte *ptr, uns len)
{
  // find shortest edge with a given prefix (may not contain data)
  TRIE_DBG("do_find_prefix('%.*s')", len, ptr);
  struct P(edge) **ref = &T.root, *edge;
  do
    {
      if (!(edge = *ref))
	return NULL;
      else if (edge->len >= len)
	return !memcmp(ptr, P(str_prefix)(P(str_get)(edge->node), edge->len, len), len) ? edge : NULL;
    }
  while (ref = P(son_find)(edge, P(str_char)(ptr, len, edge->len)));
  return NULL;
}
#endif

static struct P(edge) *
P(do_lookup)(TAC byte *ptr, uns len)
{
  TRIE_DBG("do_lookup('%.*s')", len, ptr);
  struct P(edge) **ref, *edge, *leaf, *newleaf;
  uns prefix, elen, trans, pos;
  byte *eptr;

  if (!(edge = T.root))
    {
      TRIE_DBG("Creating first edge");
      edge = T.root = P(edge_alloc)(TTC TRIE_FLAG_NODE);
      edge->node = NULL;
      edge->len = len;
      return edge;
    }
  else
    {
      while (edge->len < len && (ref = P(son_find)(edge, P(str_char)(ptr, len, edge->len))))
	edge = *ref;
      if (!(edge->flags & TRIE_FLAG_NODE))
	edge = edge->leaf;
      ASSERT(edge->flags & TRIE_FLAG_NODE);
      eptr = P(str_get)(edge->node);
      elen = edge->len;
      uns l = MIN(elen, len);
      for (prefix = 0; prefix < l; prefix++)
	if (P(str_char)(ptr, len, prefix) != P(str_char)(eptr, elen, prefix))
	  break;
      if (prefix == len && prefix == elen)
	return edge;
      TRIE_DBG("The longest common prefix is '%.*s'", prefix, P(str_prefix)(eptr, elen, prefix));

      if (prefix < len)
        {
          TRIE_DBG("Creating a new leaf");
          newleaf = P(edge_alloc)(TTC TRIE_FLAG_NODE);
          newleaf->node = NULL;
          newleaf->len = len;
	}
      else
	newleaf = NULL;

      ref = &T.root;
      while (edge = *ref)
        {
	  pos = edge->len;
	  if (prefix < pos)
	    {
	      leaf = (edge->flags & TRIE_FLAG_NODE) ? edge : edge->leaf;
	      TRIE_DBG("Splitting edge '%.*s'", leaf->len, P(str_get)(leaf->node));
	      trans = P(str_char)(P(str_get)(leaf->node), leaf->len, prefix);
	      if (len == prefix)
	        {
		  edge = P(edge_alloc)(TTC 1 | TRIE_FLAG_NODE);
		  edge->len = prefix;
		  edge->node = NULL;
		  edge->trans[0] = trans;
		  edge->son[0] = *ref;
		  return *ref = edge;
		}
	      else
	        {
		  edge = P(edge_alloc)(TTC 2);
		  edge->len = prefix;
		  edge->leaf = leaf;
		  edge->trans[0] = trans;
		  edge->son[0] = *ref;
		  edge->trans[1] = P(str_char)(ptr, len, prefix);
		  *ref = edge;
		  return edge->son[1] = newleaf;
		}
	    }
	  if (pos == len)
	    {
	      TRIE_DBG("Adding the node to an already existing edge");
	      edge->flags |= TRIE_FLAG_NODE;
	      edge->node = NULL;
	      return edge;
	    }
	  if (!(edge->flags & TRIE_FLAG_NODE) && newleaf)
	    edge->leaf = newleaf;
	  trans = P(str_char)(ptr, len, pos);
	  if (pos < prefix)
	    ref = P(son_find)(edge, trans);
	  else
	    ref = P(son_insert)(TTC ref, trans);
	}
    }
  return *ref = newleaf;
}

#ifdef TRIE_WANT_DO_DELETE
static P(node_t) *
P(do_delete)(TAC byte *ptr, uns len)
{
  TRIE_DBG("do_delete('%.*s')", len, ptr);
  struct P(edge) **ref = &T.root, **pref = NULL, *edge, *parent, *leaf, *pold = NULL;
  while (1)
    {
      if (!(edge = *ref) || edge->len > len)
	return NULL;
      else if (edge->len == len)
	if ((edge->flags & TRIE_FLAG_NODE) && !memcmp(ptr, P(str_get)(edge->node), len))
	  break;
	else
	  return NULL;
      pref = ref;
      if (!(ref = P(son_find)(edge, P(str_char)(ptr, len, edge->len))))
	return NULL;
    }

  P(node_t) *node = edge->node;
  uns deg = edge->flags & TRIE_FLAG_DEG;

  if (!deg)
    {
      if (!pref)
        {
	  TRIE_DBG("Deleting last edge");
	  T.root = NULL;
	  P(edge_free)(TTC edge);
	  return node;
	}
      else
        {
	  TRIE_DBG("Deleting a leaf");
	  pold = *pref;
	  P(son_delete)(TTC pref, P(str_char)(ptr, len, pold->len));
	  parent = *pref;
	  if ((parent->flags & (TRIE_FLAG_DEG | TRIE_FLAG_NODE)) <= 1)
	    {
	      ASSERT((parent->flags & (TRIE_FLAG_DEG | TRIE_FLAG_HASH)) == 1);
	      TRIE_DBG("... and its parent");
	      leaf = *pref = parent->son[0];
	      P(edge_free)(TTC parent);
	    }
	  else if (parent->flags & TRIE_FLAG_NODE)
	    leaf = parent;
	  else
	    leaf = P(son_any)(parent);
	}
      P(edge_free)(TTC edge);
    }
  else if (deg == 1)
    {
      TRIE_DBG("Deleting internal edge");
      ASSERT(!(edge->flags & TRIE_FLAG_HASH));
      leaf = *ref = edge->son[0];
      P(edge_free)(TTC edge);
    }
  else
    {
      TRIE_DBG("Deleting node, but leaving edge");
      leaf = P(son_any)(edge);
      if (!(leaf->flags & TRIE_FLAG_NODE))
        leaf = leaf->leaf;
      edge->leaf = leaf;
      edge->flags &= ~TRIE_FLAG_NODE;
    }

  TRIE_DBG("Updating leaf pointers");
  if (!(leaf->flags & TRIE_FLAG_NODE))
    leaf = leaf->leaf;
  ASSERT(leaf->flags & TRIE_FLAG_NODE);
  for (ref = &T.root; ref && (*ref)->len < len; ref = P(son_find)(*ref, P(str_char)(ptr, len, (*ref)->len)))
    if ((*ref)->leaf == edge || (*ref)->leaf == pold)
      (*ref)->leaf = leaf;
  return node;
}
#endif

#ifdef TRIE_WANT_FIND
static inline P(node_t) *
P(find)(TAC char *str)
{
  struct P(edge) *edge = P(do_find)(TTC str, str_len(str));
  return edge ? edge->node : NULL;
}
#endif

#ifdef TRIE_WANT_FIND_BUF
static inline P(node_t) *
P(find_buf)(TAC byte *ptr, uns len)
{
  struct P(edge) *edge = P(do_find)(TTC ptr, len);
  return edge ? edge->node : NULL;
}
#endif

#ifdef TRIE_WANT_ADD
static inline void
P(add)(TAC P(node_t) *node)
{
  struct P(edge) *edge = P(do_lookup)(TTC P(str_get)(node), P(str_len)(node));
  ASSERT(!edge->node);
  edge->node = node;
}
#endif

#ifdef TRIE_WANT_REPLACE
static inline P(node_t) *
P(replace)(TAC P(node_t) *node)
{
  struct P(edge) *edge = P(do_lookup)(TTC P(str_get)(node), P(str_len)(node));
  P(node_t) *over = edge->node;
  edge->node = node;
  return over;
}
#endif

#ifdef TRIE_WANT_DELETE
static inline P(node_t) *
P(delete)(TAC char *str)
{
  return P(do_delete)(TTC str, str_len(str));
}
#endif

#ifdef TRIE_WANT_DELETE_BUF
static inline P(node_t) *
P(delete_buf)(TAC byte *ptr, uns len)
{
  return P(do_delete)(TTC ptr, len);
}
#endif

#ifdef TRIE_WANT_REMOVE
static inline void
P(remove)(TAC P(node_t) *node)
{
  if (unlikely(P(do_delete)(TTC P(str_get)(node), P(str_len)(node)) != node))
    ASSERT(0);
}
#endif

/*** Traversing subtrees ***/

#ifdef TRIE_WANT_WALK_SUBTREE
struct P(walk) {
#ifdef TRIE_DYNAMIC
  struct P(trie) *trie;
#endif
  struct P(edge) *edge;
  void (*node_action)(struct P(walk) *walk);
  void (*edge_action)(struct P(walk) *walk);
};

static void
P(walk_subtree)(struct P(walk) *walk, struct P(edge) *edge)
{
  walk->edge = edge;
  if (walk->edge_action)
    walk->edge_action(walk);
  if ((edge->flags & TRIE_FLAG_NODE) && walk->node_action)
    walk->node_action(walk);
  if (edge->flags & TRIE_FLAG_HASH)
  {
    TRIE_HASH_FOR_ALL(edge, trans, son)
      P(walk_subtree)(walk, son);
    TRIE_HASH_END_FOR;
  }
  else
    for (uns i = edge->flags & TRIE_FLAG_DEG; i--; )
      P(walk_subtree)(walk, edge->son[i]);
}
#endif

#ifdef TRIE_WANT_WALK_ALL
static void
P(walk_all)(TAC struct P(walk) *walk)
{
#ifdef TRIE_DYNAMIC
  walk->trie = &T;
#endif
  if (T.root)
    P(walk_subtree)(walk, T.root);
}
#endif

#ifdef TRIE_WANT_WALK_PREFIX
static void
P(walk_prefix)(TAC struct P(walk) *walk, char *str)
{
  walk->trie = trie;
  struct P(edge) *edge = P(do_find_prefix)(trie, str, str_len(str));
  if (edge)
    P(walk_subtree)(walk, edge);
}
#endif

/*** Check consistency ***/

#ifdef TRIE_WANT_AUDIT

struct P(audit_walk) {
  struct P(walk) walk;
  uns count;
};

static void
P(audit_action)(struct P(walk) *walk)
{
  struct P(edge) *edge = walk->edge, *leaf;
  ASSERT(edge);
  uns deg = edge->flags & TRIE_FLAG_DEG;
  ASSERT(edge->node);
  leaf = (edge->flags & TRIE_FLAG_NODE) ? edge : edge->leaf;
  if (leaf != edge)
    {
      ASSERT(leaf->flags & TRIE_FLAG_NODE);
      ASSERT(leaf->len > edge->len);
      ASSERT(leaf->node);
    }
  TRIE_DBG("Checking edge %p, %s=%p, flags=0x%x, key='%.*s'",
      edge, (edge->flags & TRIE_FLAG_NODE) ? "node" : "leaf", edge->node, edge->flags,
      edge->len, P(str_prefix)(P(str_get)(leaf->node), leaf->len, edge->len));
  ASSERT(deg >= 2 || (edge->flags & TRIE_FLAG_NODE));
  if (edge->flags & TRIE_FLAG_HASH)
    {
      ASSERT(deg >= 1 && deg <= 256);
      uns mask = (1U << edge->hash_rank) - 1, count = 0, deleted = 0;
      for (uns i = 0; i <= mask; i++)
        {
	  struct P(bucket) *b = &edge->hash[i];
	  for (uns i = 0; i < b->count; i++)
	    if (b->son[i])
	      count++;
	    else
	      deleted++;
	}
      ASSERT(count == deg);
      ASSERT(deleted == edge->hash_deleted);
    }
  else
    ASSERT(deg <= TRIE_HASH_THRESHOLD);
  ((struct P(audit_walk) *)walk)->count++;
}

static void
P(audit)(TA)
{
  struct P(audit_walk) walk;
  bzero(&walk, sizeof(walk));
  walk.walk.edge_action = P(audit_action);
  P(walk_all)(TTC &walk.walk);
  TRIE_DBG("Found %u edges", walk.count);
}

#endif

/*** Statistics ***/

#ifdef TRIE_WANT_STATS

struct P(stats) {
  u64 total_size;
  u64 small_size;
  u64 hash_size;
};

static void
P(stats)(TAC struct P(stats) *stats)
{
  bzero(stats, sizeof(*stats));
  for (uns i = 0; i < ARRAY_SIZE(T.epool); i++)
    stats->small_size += ep_total_size(T.epool[i]);
  for (uns i = 0; i < ARRAY_SIZE(T.hpool); i++)
    stats->hash_size += ep_total_size(T.hpool[i]);
  stats->total_size = stats->small_size + stats->total_size + sizeof(T);
}

static inline u64
P(total_size)(TA)
{
  struct P(stats) stats;
  P(stats)(TTC &stats);
  return stats.total_size;
}

#endif

/*** Clean up local macros ***/

#undef P
#undef T
#undef TA
#undef TAC
#undef TT
#undef TTC

#undef TRIE_PREFIX
#undef TRIE_NODE_TYPE
#undef TRIE_NODE_KEY
#undef TRIE_NODE_LEN
#undef TRIE_LEN_TYPE
#undef TRIE_REV
#undef TRIE_DYNAMIC
#undef TRIE_ELTPOOL_SIZE
#undef TRIE_HASH_THRESHOLD
#undef TRIE_BUCKET_SIZE
#undef TRIE_TRACE
#undef TRIE_DBG
#undef TRIE_HASH_FOR_ALL
#undef TRIE_HASH_END_FOR

#undef TRIE_WANT_CLEANUP

#undef TRIE_WANT_DO_FIND
#undef TRIE_WANT_DO_FIND_PREFIX
#undef TRIE_WANT_DO_LOOKUP
#undef TRIE_WANT_DO_DELETE

#undef TRIE_WANT_FIND
#undef TRIE_WANT_FIND_BUF
#undef TRIE_WANT_ADD
#undef TRIE_WANT_ADD_OVER
#undef TRIE_WANT_DELETE
#undef TRIE_WANT_DELETE_BUF
#undef TRIE_WANT_REMOVE

#undef TRIE_WANT_WALK_SUBTREE
#undef TRIE_WANT_WALK_ALL
#undef TRIE_WANT_WALK_PREFIX
#undef TRIE_WANT_AUDIT
