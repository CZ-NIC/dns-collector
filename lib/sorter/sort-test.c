/* A test of sorting routines */

#include "lib/lib.h"
#include "lib/getopt.h"
#include "lib/fastbuf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

struct key {
  uns x;
};

static inline void s_write_merged(struct fastbuf *f, struct key **k, void **d, uns n, void *buf)
{
  bwrite(f, k[0], sizeof(struct key));
  bwrite(f, d[0], 5);
}

static inline void s_copy_merged(struct key **keys, struct fastbuf **data, uns n, struct fastbuf *dest)
{
  bwrite(dest, keys[0], sizeof(struct key));
  bbcopy(data[0], dest, 5);
  for (uns i=1; i<n; i++)
    bskip(data[i], 5);
}

#define SORT_KEY_REGULAR struct key
#define SORT_PREFIX(x) s_##x
#define SORT_INPUT_FB
#define SORT_OUTPUT_FB
//#define SORT_KEY_SIZE(k) 4
#define SORT_DATA_SIZE(k) 5
//#define SORT_UNIQUE
#define SORT_UNIFY
#define SORT_INT(k) (k).x

#include "lib/sorter/sorter.h"

int
main(int argc, char **argv)
{
  log_init(NULL);
  if (cf_getopt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) >= 0 ||
      optind != argc - 2)
  {
    fputs("This program supports only the following command-line arguments:\n" CF_USAGE, stderr);
    exit(1);
  }

  log(L_INFO, "Generating");
  struct fastbuf *f = bopen(argv[optind], O_RDWR | O_CREAT | O_TRUNC, 65536);
#define N 259309
#define K 199483
  for (uns i=0; i<2*N; i++)
    {
      bputl(f, ((u64)i * K + 17) % N);
      bputs(f, "12345");
      bputl(f, ((u64)i * K + 17) % N);
      bputs(f, "12345");
    }
  brewind(f);

  log(L_INFO, "Sorting");
  f = s_sort(f, NULL, N-1);

  log(L_INFO, "Verifying");
  for (uns i=0; i<N; i++)
    {
      uns j = bgetl(f);
      if (i != j)
	die("Discrepancy: %d instead of %d", j, i);
      for (uns i='1'; i<='5'; i++)
	if (bgetc(f) != i)
	  ASSERT(0);
    }
  bclose(f);

  return 0;
}
