/*
 *	Sherlock Library -- Bucket Manipulation Tool
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/bucket.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"
#include "lib/conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

static int verbose;

static void
help(void)
{
  fprintf(stderr, "\
Usage: buckettool [<options>] <command>\n\
\n\
Options:\n"
CF_USAGE
"\nCommands:\n\
-l\t\tlist all buckets\n\
-L\t\tlist all buckets including deleted ones\n\
-d <obj>\tdelete bucket\n\
-x <obj>\textract bucket\n\
-i[<type>]\tinsert buckets separated by blank lines\n\
-c\t\tconcatenate and dump all buckets\n\
-f\t\taudit bucket file structure\n\
-F\t\taudit and fix bucket file structure\n\
-q\t\tquick check of bucket file consistency\n\
-s\t\tshake down bucket file (without updating other structures!!!)\n\
-v\t\tbe verbose\n\
");
  exit(1);
}

static oid_t
parse_id(char *c)
{
  char *e;
  oid_t o = strtoul(c, &e, 16);
  if (e && *e)
    die("Invalid object ID: %s", c);
  return o;
}

static void
list(int full)
{
  struct obuck_header h;

  obuck_init(0);
  if (obuck_find_first(&h, full))
    do
      {
	if (h.oid == OBUCK_OID_DELETED)
	  printf("DELETED  %6d\n", h.length);
	else
	  printf("%08x %6d %08x\n", h.oid, h.length, h.type);
      }
    while (obuck_find_next(&h, full));
  obuck_cleanup();
}

static void
delete(char *id)
{
  oid_t oid = parse_id(id);
  obuck_init(1);
  obuck_delete(oid);
  obuck_cleanup();
}

static void
extract(char *id)
{
  struct fastbuf *b;
  byte buf[1024];
  int l;
  struct obuck_header h;

  h.oid = parse_id(id);
  obuck_init(0);
  obuck_find_by_oid(&h);
  b = obuck_fetch();
  while ((l = bread(b, buf, sizeof(buf))))
    fwrite(buf, 1, l, stdout);
  obuck_fetch_end(b);
  obuck_cleanup();
}

static void
insert(byte *arg)
{
  struct fastbuf *b, *in;
  byte buf[4096];
  struct obuck_header h;
  byte *e;
  u32 type;

  if (!arg)
    type = BUCKET_TYPE_PLAIN;
  else if (sscanf(arg, "%x", &type) != 1)
    die("Type `%s' is not a hexadecimal number");

  in = bfdopen_shared(0, 4096);
  obuck_init(1);
  do
    {
      b = obuck_create(type);
      while ((e = bgets(in, buf, sizeof(buf))) && buf[0])
	{
	  *e++ = '\n';
	  bwrite(b, buf, e-buf);
	}
      obuck_create_end(b, &h);
      printf("%08x %d %08x\n", h.oid, h.length, h.type);
    }
  while (e);
  obuck_cleanup();
  bclose(in);
}

static void
cat(void)
{
  struct obuck_header h;
  struct fastbuf *b;
  byte buf[1024];
  int l, lf;

  obuck_init(0);
  while (b = obuck_slurp_pool(&h))
    {
      printf("### %08x %6d %08x\n", h.oid, h.length, h.type);
      lf = 1;
      while ((l = bread(b, buf, sizeof(buf))))
	{
	  fwrite(buf, 1, l, stdout);
	  lf = (buf[l-1] == '\n');
	}
      if (!lf)
	printf("\n# <missing EOL>\n");
    }
  obuck_cleanup();
}

static void
fsck(int fix)
{
  int fd, i;
  struct obuck_header h, nh;
  sh_off_t pos = 0;
  sh_off_t end;
  oid_t oid;
  u32 chk;
  int errors = 0;
  int fatal_errors = 0;

  fd = sh_open(obuck_name, O_RDWR);
  if (fd < 0)
    die("Unable to open the bucket file %s: %m", obuck_name);
  for(;;)
    {
      oid = pos >> OBUCK_SHIFT;
      i = sh_pread(fd, &h, sizeof(h), pos);
      if (!i)
	break;
      if (i != sizeof(h))
	printf("%08x  incomplete header\n", oid);
      else if (h.magic == OBUCK_INCOMPLETE_MAGIC)
	printf("%08x  incomplete file\n", oid);
      else if (h.magic != OBUCK_MAGIC)
	printf("%08x  invalid header magic\n", oid);
      else if (h.oid != oid && h.oid != OBUCK_OID_DELETED)
	printf("%08x  invalid header backlink\n", oid);
      else
	{
	  end = (pos + sizeof(h) + h.length + 4 + OBUCK_ALIGN - 1) & ~(sh_off_t)(OBUCK_ALIGN - 1);
	  if (sh_pread(fd, &chk, 4, end-4) != 4)
	    printf("%08x  missing trailer\n", oid);
	  else if (chk != OBUCK_TRAILER)
	    printf("%08x  mismatched trailer\n", oid);
	  else
	    {
	      /* OK */
	      pos = end;
	      continue;
	    }
	}
      errors++;
      end = pos;
      do
	{
	  if (pos - end > 0x10000000)
	    {
	      printf("*** skipped for too long, giving up\n");
	      fatal_errors++;
	      goto finish;
	    }
	  end += OBUCK_ALIGN;
	  if (sh_pread(fd, &nh, sizeof(nh), end) != sizeof(nh))
	    {
	      printf("*** unable to find next header\n");
	      if (fix)
		{
		  printf("*** truncating file\n");
		  sh_ftruncate(fd, pos);
		}
	      else
		printf("*** would truncate the file here\n");
	      goto finish;
	    }
	}
      while (nh.magic != OBUCK_MAGIC ||
	     (nh.oid != (oid_t)(end >> OBUCK_SHIFT) && nh.oid != OBUCK_OID_DELETED));
      printf("*** match at oid %08x\n", (uns)(end >> OBUCK_SHIFT));
      if (fix)
	{
	  h.magic = OBUCK_MAGIC;
	  h.oid = OBUCK_OID_DELETED;
	  h.length = end - pos - sizeof(h) - 4;
	  sh_pwrite(fd, &h, sizeof(h), pos);
	  chk = OBUCK_TRAILER;
	  sh_pwrite(fd, &chk, 4, end-4);
	  printf("*** replaced the invalid chunk by a DELETED bucket of size %d\n", (uns)(end - pos));
	}
      else
	printf("*** would mark %d bytes as DELETED\n", (uns)(end - pos));
      pos = end;
    }
 finish:
  close(fd);
  if (!fix && errors || fatal_errors)
    exit(1);
}

static int
shake_kibitz(struct obuck_header *old, oid_t new, byte *buck UNUSED)
{
  if (verbose)
    {
      printf("%08x -> ", old->oid);
      if (new == OBUCK_OID_DELETED)
	puts("DELETED");
      else
	printf("%08x\n", new);
    }
  return 1;
}

static void
shake(void)
{
  obuck_init(1);
  obuck_shakedown(shake_kibitz);
  obuck_cleanup();
}

static void
quickcheck(void)
{
  obuck_init(1);
  obuck_cleanup();
}

int
main(int argc, char **argv)
{
  int i, op;
  char *arg = NULL;

  log_init(NULL);
  op = 0;
  while ((i = cf_getopt(argc, argv, CF_SHORT_OPTS "lLd:x:i::cfFqsv", CF_NO_LONG_OPTS, NULL)) != -1)
    if (i == '?' || op)
      help();
    else if (i == 'v')
      verbose++;
    else
      {
	op = i;
	arg = optarg;
      }
  if (optind < argc)
    help();

  switch (op)
    {
    case 'l':
      list(0);
      break;
    case 'L':
      list(1);
      break;
    case 'd':
      delete(arg);
      break;
    case 'x':
      extract(arg);
      break;
    case 'i':
      insert(arg);
      break;
    case 'c':
      cat();
      break;
    case 'f':
      fsck(0);
      break;
    case 'F':
      fsck(1);
      break;
    case 'q':
      quickcheck();
      break;
    case 's':
      shake();
      break;
    default:
      help();
    }

  return 0;
}
