/*
 *	SDBM Database Recovery Utility
 *
 *	(c) 2000 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "db.c"

int
main(int argc, char **argv)
{
  struct sdbm *src, *dest;
  struct sdbm_options op;
  int e, c=0;

  if (argc != 3)
    die("Usage: db-rebuild <src> <dest>");

  bzero(&op, sizeof(op));
  op.name = argv[1];
  op.cache_size = 16;
  op.flags = 0;
  src = sdbm_open(&op);
  if (!src)
    die("Source open failed");

  op.name = argv[2];
  e = unlink(op.name);
  if (e < 0 && errno != ENOENT)
    die("unlink: %m");
  op.cache_size = 1024;
  op.flags = SDBM_CREAT | SDBM_WRITE | SDBM_FAST;
  op.page_order = src->root->page_order;
  op.key_size = src->root->key_size;
  op.val_size = src->root->val_size;
  dest = sdbm_open(&op);
  if (!dest)
    die("Destination open failed");

  puts("Rebuilding database...");
  sdbm_rewind(src);
  for(;;)
    {
      byte key[4096], val[4096];
      int klen = sizeof(key);
      int vlen = sizeof(val);
      e = sdbm_get_next(src, key, &klen, val, &vlen);
      if (!e)
	break;
      if (e < 0)
	printf("sdbm_get_next: error %d\n", e);
      if (!(c++ % 1024))
	{
	  printf("%d\r", c);
	  fflush(stdout);
	}
      if (sdbm_store(dest, key, klen, val, vlen) == 0)
	printf("sdbm_store: duplicate key\n");
    }

  sdbm_close(src);
  sdbm_close(dest);
  printf("Copied %d keys\n", c);
  return 0;
}
