#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"
#include "lib/lizard.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/user.h>

static char *options = CF_SHORT_OPTS "cdtx";
static char *help = "\
Usage: lizard-test <options> input-file [output-file]\n\
\n\
Options:\n"
CF_USAGE
"-c\t\tCompress (default)\n\
-d\t\tDecompress\n\
-t\t\tCompress, decompress, and compare (in memory only)\n\
-x, -xx, -xxx\tMake the test crash by underestimating the output buffer\n\
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
  uns crash = 0;
  log_init(argv[0]);
  while ((opt = cf_getopt(argc, argv, options, CF_NO_LONG_OPTS, NULL)) >= 0)
    switch (opt)
    {
      case 'c':
      case 'd':
      case 't':
	action = opt;
	break;
      case 'x':
	crash++;
	break;
      default:
	usage();
    }
  if (action == 't' && argc != optind+1
  || action != 't' && argc != optind+2)
    usage();

  void *mi, *mo;
  int li, lo;

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
    struct lizard_buffer *buf = lizard_alloc(li);
    uns exp_len = li;
    if (crash==1)
    {
      if (exp_len >= PAGE_SIZE)
	exp_len -= PAGE_SIZE;
      else
	exp_len = 0;
      printf("Setting shorter output buffer to %d: ", exp_len);
      fflush(stdout);
    }
    int lv = lizard_decompress_safe(mo, buf, exp_len);
    printf("-> %d ", lv);
    fflush(stdout);
    if (crash >= 2)
    {
      uns guarded_pos = (lv + PAGE_SIZE-1)/PAGE_SIZE * PAGE_SIZE;
      printf("Reading from guarded memory: %d\n", ((byte *) (buf->ptr)) [guarded_pos]);
      if (crash == 2)
      {
	printf("Writing to guarded memory: ");
	fflush(stdout);
	((byte *) (buf->ptr)) [guarded_pos] = 0;
	printf("succeeded\n");
      }
      if (crash == 3)
      {
	printf("Reading behind guarded memory: ");
	fflush(stdout);
	printf("%d\n", ((byte *) (buf->ptr)) [guarded_pos + PAGE_SIZE] = 0);
      }
    }
    if (lv != li || memcmp(mi, buf->ptr, li))
      printf("WRONG");
    else
      printf("OK");
    lizard_free(buf);
  }
  printf("\n");
}
