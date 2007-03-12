/*
 *	SDBM Database Utility
 *
 *	(c) 2000--2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/db.h"
#include "lib/db_internal.h"
#include "lib/fastbuf.h"
#include "lib/ff-binary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static int verbose=0;
static int cache=1024;
static int force_key=-2;
static int force_val=-2;
static int force_page=-1;

#define SDBM_DUMP_MAGIC 0x321f120e
#define SDBM_DUMP_VERSION 1

static void
dump(char *db, char *dmp)
{
  struct sdbm *src;
  struct fastbuf *dest;
  struct sdbm_options op;
  int e, c=0;

  bzero(&op, sizeof(op));
  op.name = db;
  op.cache_size = 16;
  op.flags = 0;
  src = sdbm_open(&op);
  if (!src)
    die("Source open failed: %m");

  dest = bopen(dmp, O_WRONLY | O_CREAT | O_TRUNC, 65536);
  bputl(dest, SDBM_DUMP_MAGIC);
  bputl(dest, SDBM_DUMP_VERSION);
  bputl(dest, src->page_order);
  bputl(dest, src->key_size);
  bputl(dest, src->val_size);

  fprintf(stderr, "Dumping database...\n");
  sdbm_rewind(src);
  for(;;)
    {
      byte key[65536], val[65536];
      int klen = sizeof(key);
      int vlen = sizeof(val);
      e = sdbm_get_next(src, key, &klen, val, &vlen);
      if (!e)
	break;
      if (e < 0)
	fprintf(stderr, "sdbm_get_next: error %d\n", e);
      if (!(c++ % 1024))
	{
	  fprintf(stderr, "%d\r", c);
	  fflush(stderr);
	}
      bputw(dest, klen);
      bwrite(dest, key, klen);
      bputw(dest, vlen);
      bwrite(dest, val, vlen);
    }

  sdbm_close(src);
  bclose(dest);
  fprintf(stderr, "Dumped %d records\n", c);
}

static void
restore(char *dmp, char *db)
{
  struct sdbm *dest;
  struct fastbuf *src;
  struct sdbm_options op;
  int e, c=0;

  src = bopen(dmp, O_RDONLY, 65536);
  if (bgetl(src) != SDBM_DUMP_MAGIC ||
      bgetl(src) != SDBM_DUMP_VERSION)
    die("%s: not a sdbm dump", dmp);

  bzero(&op, sizeof(op));
  op.name = db;
  e = unlink(op.name);
  if (e < 0 && errno != ENOENT)
    die("unlink: %m");
  op.cache_size = cache;
  op.flags = SDBM_CREAT | SDBM_WRITE | SDBM_FAST;
  op.page_order = bgetl(src);
  if (force_page >= 0)
    op.page_order = force_page;
  op.key_size = bgetl(src);
  if (force_key >= 0)
    op.key_size = force_key;
  op.val_size = bgetl(src);
  if (force_val >= 0)
    op.val_size = force_val;
  dest = sdbm_open(&op);
  if (!dest)
    die("Destination open failed");

  fprintf(stderr, "Restoring database...\n");
  for(;;)
    {
      byte key[65536], val[65536];
      int klen, vlen;
      klen = bgetw(src);
      if (klen < 0)
	break;
      breadb(src, key, klen);
      vlen = bgetw(src);
      if (vlen < 0)
	die("Corrupted dump file: value missing");
      breadb(src, val, vlen);
      if (!(c++ % 1024))
	{
	  fprintf(stderr, "%d\r", c);
	  fflush(stderr);
	}
      if (sdbm_store(dest, key, klen, val, vlen) == 0)
	fprintf(stderr, "sdbm_store: duplicate key\n");
    }

  bclose(src);
  sdbm_close(dest);
  fprintf(stderr, "Restored %d records\n", c);
}

static void
rebuild(char *sdb, char *ddb)
{
  struct sdbm *src, *dest;
  struct sdbm_options op;
  int e, c=0;

  bzero(&op, sizeof(op));
  op.name = sdb;
  op.cache_size = 16;
  op.flags = 0;
  src = sdbm_open(&op);
  if (!src)
    die("Source open failed: %m");

  op.name = ddb;
  e = unlink(op.name);
  if (e < 0 && errno != ENOENT)
    die("unlink: %m");
  op.cache_size = cache;
  op.flags = SDBM_CREAT | SDBM_WRITE | SDBM_FAST;
  op.page_order = (force_page >= 0) ? (u32) force_page : src->root->page_order;
  op.key_size = (force_key >= -1) ? force_key : src->root->key_size;
  op.val_size = (force_val >= -1) ? force_val : src->root->val_size;
  dest = sdbm_open(&op);
  if (!dest)
    die("Destination open failed");

  fprintf(stderr, "Rebuilding database...\n");
  sdbm_rewind(src);
  for(;;)
    {
      byte key[65536], val[65536];
      int klen = sizeof(key);
      int vlen = sizeof(val);
      e = sdbm_get_next(src, key, &klen, val, &vlen);
      if (!e)
	break;
      if (e < 0)
	fprintf(stderr, "sdbm_get_next: error %d\n", e);
      if (!(c++ % 1024))
	{
	  fprintf(stderr, "%d\r", c);
	  fflush(stderr);
	}
      if (sdbm_store(dest, key, klen, val, vlen) == 0)
	fprintf(stderr, "sdbm_store: duplicate key\n");
    }

  sdbm_close(src);
  sdbm_close(dest);
  fprintf(stderr, "Copied %d records\n", c);
}

int
main(int argc, char **argv)
{
  int o;

  while ((o = getopt(argc, argv, "vc:k:d:p:")) >= 0)
    switch (o)
      {
      case 'v':
	verbose++;
	break;
      case 'c':
	cache=atol(optarg);
	break;
      case 'k':
	force_key=atol(optarg);
	break;
      case 'd':
	force_val=atol(optarg);
	break;
      case 'p':
	force_page=atol(optarg);
	break;
      default:
      bad:
        fprintf(stderr, "Usage: db-tool [<options>] <command> <database>\n\
\n\
Options:\n\
-v\t\tBe verbose\n\
-c<n>\t\tUse cache of <n> pages\n\
-d<n>\t\tSet data size to <n> (-1=variable) [restore,rebuild]\n\
-k<n>\t\tSet key size to <n> (-1=variable) [restore,rebuild]\n\
-p<n>\t\tSet page order to <n> [restore,rebuild]\n\
\n\
Commands:\n\
b <db> <new>\tRebuild database\n\
d <db> <dump>\tDump database\n\
r <dump> <db>\tRestore database from dump\n\
");
	return 1;
      }
  argc -= optind;
  argv += optind;
  if (argc < 1 || strlen(argv[0]) != 1)
    goto bad;

  switch (argv[0][0])
    {
    case 'b':
      if (argc != 3)
	goto bad;
      rebuild(argv[1], argv[2]);
      break;
    case 'd':
      if (argc != 3)
	goto bad;
      dump(argv[1], argv[2]);
      break;
    case 'r':
      if (argc != 3)
	goto bad;
      restore(argv[1], argv[2]);
      break;
    default:
      goto bad;
    }
  return 0;
}
