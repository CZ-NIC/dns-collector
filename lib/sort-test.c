/* Test for sorting routines */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"

#include <stdio.h>
#include <string.h>

struct key {
  char line[1024];
};

#define SORT_KEY struct key
#define SORT_PREFIX(x) s_##x
#define SORT_INPUT_FILE
#define SORT_OUTPUT_FILE

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

#include "lib/sorter.h"

int
main(int argc, char **argv)
{
  log_init(NULL);
  cf_read(DEFAULT_CONFIG);
  if (cf_getopt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) >= 0 ||
      optind != argc - 2)
    die("Usage: sort-test <input> <output>");

  s_sort(argv[optind], argv[optind+1]);
  return 0;
}
