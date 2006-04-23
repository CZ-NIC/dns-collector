/* Test for sorting routines */

#include "lib/lib.h"
#include "lib/conf2.h"
#include "lib/fastbuf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

struct key {
  char line[4096];
};

#define SORT_KEY struct key
#define SORT_PREFIX(x) s_##x
#define SORT_PRESORT
#define SORT_INPUT_FILE
#define SORT_OUTPUT_FILE
#define SORT_UNIFY

static inline int
s_compare(struct key *a, struct key *b)
{
  return strcmp(a->line, b->line);
}

static inline int
s_fetch_key(struct fastbuf *f, struct key *a)
{
  return !!bgets(f, a->line, sizeof(a->line));
}

static inline void
s_copy_data(struct fastbuf *src UNUSED, struct fastbuf *dest, struct key *k)
{
  bputsn(dest, k->line);
}

static inline byte *
s_fetch_item(struct fastbuf *src UNUSED, struct key *k, byte *limit UNUSED)
{
  byte *end = (byte *) k->line + strlen(k->line) + 1;
#if 0					/* Testing splits */
  uns r = random_max(10000);
  if (end + r <= limit)
    return end + r;
  else
    return NULL;
#else
  return end;
#endif
}

static inline void
s_store_item(struct fastbuf *f, struct key *k)
{
  s_copy_data(NULL, f, k);
}

#ifdef SORT_UNIFY
static inline void
s_merge_data(struct fastbuf *src1 UNUSED, struct fastbuf *src2 UNUSED, struct fastbuf *dest, struct key *k1, struct key *k2 UNUSED)
{
  s_copy_data(NULL, dest, k1);
}

static inline struct key *
s_merge_items(struct key *a, struct key *b UNUSED)
{
  return a;
}
#endif

#include "lib/sorter.h"

int
main(int argc, char **argv)
{
  log_init(NULL);
  if (cf_get_opt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) >= 0 ||
      optind != argc - 2)
  {
    fputs("This program supports only the following command-line arguments:\n" CF_USAGE, stderr);
    exit(1);
  }

  s_sort(argv[optind], argv[optind+1]);
  return 0;
}
