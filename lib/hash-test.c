/* Tests for hash table routines */

#include "lib/lib.h"

#include <stdio.h>
#include <string.h>

#if 1

/* TEST 1: integers */

struct node {
  int key;
  int data;
};

#define HASH_NODE struct node
#define HASH_PREFIX(x) test_##x
#define HASH_KEY_ATOMIC key
#define HASH_ATOMIC_TYPE int

#define HASH_GIVE_INIT_DATA
static inline void test_init_data(struct node *n)
{
  n->data = n->key + 123;
}

#define HASH_WANT_FIND
//#define HASH_WANT_NEW
#define HASH_WANT_LOOKUP
//#define HASH_WANT_DELETE
#define HASH_WANT_REMOVE

#include "lib/hashtable.h"

static void test(void)
{
  int i;

  test_init();
  for (i=0; i<1024; i++)
    {
      struct node *n = test_lookup(i);
      ASSERT(n->data == i+123);
    }
  for (i=1; i<1024; i+=2)
    {
#if 0
      test_delete(i);
#else
      struct node *n = test_lookup(i);
      test_remove(n);
#endif
    }
  for (i=0; i<1024; i++)
    {
      struct node *n = test_find(i);
      if (!n != (i&1) || (n && n->data != i+123))
	die("Inconsistency at i=%d", i);
    }
  i=0;
  HASH_FOR_ALL(test, n)
    {
      i += 1 + 0*n->key;
      // printf("%d\n", n->key);
    }
  HASH_END_FOR;
  ASSERT(i == 512);
  log(L_INFO, "OK");
}

#elif 0

/* TEST 2: external strings */

struct node {
  char *key;
  int data;
};

#define HASH_NODE struct node
#define HASH_PREFIX(x) test_##x
#define HASH_KEY_STRING key
#define HASH_NOCASE

#define HASH_WANT_FIND
#define HASH_WANT_NEW

#include "lib/hashtable.h"

static void test(void)
{
  int i;

  test_init();
  for (i=0; i<1024; i+=2)
    {
      char x[32];
      sprintf(x, "abc%d", i);
      test_new(stralloc(x));
    }
  for (i=0; i<1024; i++)
    {
      char x[32];
      struct node *n;
      sprintf(x, "ABC%d", i);
      n = test_find(x);
      if (!n != (i&1))
	die("Inconsistency at i=%d", i);
    }
  log(L_INFO, "OK");
}

#elif 0

/* TEST 3: internal strings + pools */

#include "lib/pools.h"

static struct mempool *pool;

struct node {
  int data;
  char key[1];
};

#define HASH_NODE struct node
#define HASH_PREFIX(x) test_##x
#define HASH_KEY_ENDSTRING key

#define HASH_WANT_FIND
#define HASH_WANT_NEW

#define HASH_USE_POOL pool

#include "lib/hashtable.h"

static void test(void)
{
  int i;

  pool = mp_new(16384);
  test_init();
  for (i=0; i<1024; i+=2)
    {
      char x[32];
      sprintf(x, "abc%d", i);
      test_new(x);
    }
  for (i=0; i<1024; i++)
    {
      char x[32];
      struct node *n;
      sprintf(x, "abc%d", i);
      n = test_find(x);
      if (!n != (i&1))
	die("Inconsistency at i=%d", i);
    }
  log(L_INFO, "OK");
}

#elif 1

/* TEST 4: complex keys */

#include "lib/hashfunc.h"

struct node {
  int port;
  int data;
  char host[1];
};

#define HASH_NODE struct node
#define HASH_PREFIX(x) test_##x
#define HASH_KEY_COMPLEX(x) x host, x port
#define HASH_KEY_DECL char *host, int port

#define HASH_WANT_CLEANUP
#define HASH_WANT_FIND
#define HASH_WANT_NEW
#define HASH_WANT_LOOKUP
#define HASH_WANT_DELETE
#define HASH_WANT_REMOVE

#define HASH_GIVE_HASHFN
static uns test_hash(char *host, int port)
{
  return hash_string_nocase(host) ^ hash_int(port);
}

#define HASH_GIVE_EQ
static inline int test_eq(char *host1, int port1, char *host2, int port2)
{
  return !strcasecmp(host1,host2) && port1 == port2;
}

#define HASH_GIVE_EXTRA_SIZE
static inline uns test_extra_size(char *host, int port UNUSED)
{
  return strlen(host);
}

#define HASH_GIVE_INIT_KEY
static inline void test_init_key(struct node *n, char *host, int port)
{
  strcpy(n->host, host);
  n->port = port;
}

#include "lib/hashtable.h"

static void test(void)
{
  int i;

  test_init();
  for (i=0; i<1024; i+=2)
    {
      char x[32];
      sprintf(x, "abc%d", i);
      test_new(x, i%10);
    }
  for (i=0; i<1024; i++)
    {
      char x[32];
      struct node *n;
      sprintf(x, "ABC%d", i);
      n = test_find(x, i%10);
      if (!n != (i&1))
	die("Inconsistency at i=%d", i);
    }
  log(L_INFO, "OK");
  test_cleanup();
  log(L_INFO, "Cleaned up");
}

#endif

int
main(void)
{
  test();
  return 0;
}
