#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"
#include "lib/lizard.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

static char *options = CF_SHORT_OPTS "cdt";
static char *help = "\
Usage: lizard-test <options> input-file [output-file]\n\
\n\
Options:\n"
CF_USAGE
"-c\t\tCompress (default)\n\
-d\t\tDecompress\n\
-t\t\tCompress, decompress, and compare (in memory only)\n\
";

static void NONRET
usage(void)
{
  fputs(help, stderr);
  exit(1);
}

int
main(int argc, char **argv)
{
  int opt;
  uns action = 'c';
  log_init(argv[0]);
  while ((opt = cf_getopt(argc, argv, options, CF_NO_LONG_OPTS, NULL)) >= 0)
    switch (opt)
    {
      case 'c':
      case 'd':
      case 't':
	action = opt;
	break;
      default:
	usage();
    }
  if (action == 't' && argc != optind+1
  || action != 't' && argc != optind+2)
    usage();

  void *mi, *mo;
  uns li, lo;

  struct stat st;
  stat(argv[optind], &st);
  li = st.st_size;
  struct fastbuf *fi = bopen(argv[optind], O_RDONLY, 1<<16);
  if (action != 'd')
  {
    lo = li * LIZARD_MAX_MULTIPLY + LIZARD_MAX_ADD;
    li += LIZARD_NEEDS_CHARS;
  }
  else
  {
    lo = bgetl(fi);
    li -= 4;
  }
  mi = xmalloc(li);
  mo = xmalloc(lo);
  li = bread(fi, mi, li);
  bclose(fi);

  printf("%d ", li);
  if (action == 'd')
    printf("->expected %d ", lo);
  fflush(stdout);
  if (action != 'd')
    lo = lizard_compress(mi, li, mo);
  else
    lo = lizard_decompress(mi, mo);
  printf("-> %d ", lo);
  fflush(stdout);

  if (action != 't')
  {
    struct fastbuf *fo = bopen(argv[optind+1], O_CREAT | O_TRUNC | O_WRONLY, 1<<16);
    if (action == 'c')
      bputl(fo, li);
    bwrite(fo, mo, lo);
    bclose(fo);
  }
  else
  {
    void *mv;
    uns lv;
    mv = xmalloc(li);
    lv = lizard_decompress(mo, mv);
    printf("-> %d ", lv);
    fflush(stdout);
    if (lv != li || memcmp(mi, mv, lv))
      printf("WRONG");
    else
      printf("OK");
  }
  printf("\n");
}
