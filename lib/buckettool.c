/*
 *	Sherlock Library -- Bucket Manipulation Tool
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/bucket.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

static void
help(void)
{
  fprintf(stderr, "\
Usage: buckettool <commands>\n\
\n\
Commands:\n\
-l\t\tlist all buckets\n\
-L\t\tlist all buckets including deleted ones\n\
-d <obj>\tdelete bucket\n\
-x <obj>\textract bucket\n\
-i\t\tinsert bucket\n\
-c\t\tconcatenate and dump all buckets\n\
-f\t\taudit bucket file structure\n\
-F\t\taudit and fix bucket file structure\n\
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
	  printf("%08x %6d %6d\n", h.oid, h.length, h.orig_length);
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
insert(void)
{
  struct fastbuf *b;
  byte buf[1024];
  int l;
  struct obuck_header h;

  obuck_init(1);
  b = obuck_create();
  while ((l = fread(buf, 1, sizeof(buf), stdin)))
    bwrite(b, buf, l);
  obuck_create_end(b, &h);
  obuck_cleanup();
  printf("%08x %d %d\n", h.oid, h.length, h.orig_length);
}

static void
cat(void)
{
  struct obuck_header h;
  struct fastbuf *b;
  byte buf[1024];
  int l, lf;

  obuck_init(0);
  if (obuck_find_first(&h, 0))
    do
      {
	printf("### %08x %6d %6d\n", h.oid, h.length, h.orig_length);
	b = obuck_fetch();
	lf = 1;
	while ((l = bread(b, buf, sizeof(buf))))
	  {
	    fwrite(buf, 1, l, stdout);
	    lf = (buf[l-1] == '\n');
	  }
	obuck_fetch_end(b);
	if (!lf)
	  printf("\n# <missing EOL>\n");
      }
    while (obuck_find_next(&h, 0));
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

  fd = open(obuck_name, O_RDWR);
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
	  /* OK */
	  pos = end;
	  continue;
	}
      end = pos;
      do
	{
	  if (pos - end > 0x10000000)
	    {
	      printf("*** skipped for too long, giving up\n");
	      goto finish;
	    }
	  end += OBUCK_ALIGN;
	  if (sh_pread(fd, &nh, sizeof(nh), end) != sizeof(nh))
	    {
	      printf("*** unable to find next header\n");
	      if (fix)
		{
		  printf("*** truncating file\n");
		  ftruncate(fd, pos);
		}
	      else
		printf("*** would truncate the file here\n");
	      goto finish;
	    }
	}
      while (nh.magic != OBUCK_MAGIC ||
	     (nh.oid != (oid_t)(end >> OBUCK_SHIFT) && nh.oid != OBUCK_OID_DELETED));
      printf("*** match at oid %08x\n", end >> OBUCK_SHIFT);
      if (fix)
	{
	  h.magic = OBUCK_MAGIC;
	  h.oid = OBUCK_OID_DELETED;
	  h.length = h.orig_length = end - pos - sizeof(h) - 4;
	  sh_pwrite(fd, &h, sizeof(h), pos);
	  chk = OBUCK_TRAILER;
	  sh_pwrite(fd, &chk, 4, end-4);
	  printf("*** replaced the invalid chunk by a DELETED bucket of size %d\n", end - pos);
	}
      pos = end;
    }
 finish:
  close(fd);
}

int
main(int argc, char **argv)
{
  int i, op;

  op = 0;
  while ((i = getopt(argc, argv, "lLd:x:icfF")) != -1)
    switch (i)
      {
      case 'l':
	list(0);
	op++;
	break;
      case 'L':
	list(1);
	op++;
	break;
      case 'd':
	delete(optarg);
	op++;
	break;
      case 'x':
	extract(optarg);
	op++;
	break;
      case 'i':
	insert();
	op++;
	break;
      case 'c':
	cat();
	op++;
	break;
      case 'f':
	fsck(0);
	op++;
	break;
      case 'F':
	fsck(1);
	op++;
	break;
      default:
	help();
      }
  if (optind < argc || !op)
    help();

  return 0;
}
