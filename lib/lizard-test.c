#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/lizzard.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
  if (argc < 4)
    die("Syntax: lizzard-test -cd input-file output-file");
  uns compress = !strcmp(argv[1], "-c");
  struct fastbuf *fi, *fo;
  void *mi, *mo;
  uns li, lo;

  struct stat st;
  stat(argv[2], &st);
  li = st.st_size;
  fi = bopen(argv[2], O_RDONLY, 1<<16);
  if (compress)
  {
    lo = li * LIZZARD_MAX_MULTIPLY + LIZZARD_MAX_ADD;
    li += LIZZARD_NEEDS_CHARS;
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
  if (!compress)
    printf("->expected %d ", lo);
  fflush(stdout);
  if (compress)
    lo = lizzard_compress(mi, li, mo);
  else
    lo = lizzard_decompress(mi, mo);
  printf("-> %d\n", lo);
  fflush(stdout);

  fo = bopen(argv[3], O_CREAT | O_TRUNC | O_WRONLY, 1<<16);
  if (compress)
    bputl(fo, li);
  bwrite(fo, mo, lo);
  bclose(fo);
}
