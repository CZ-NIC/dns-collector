/*
 *	Sherlock Library -- Bucket Manipulation Tool
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/bucket.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"
#include "lib/conf.h"
#include "lib/pools.h"
#include "lib/object.h"
#include "lib/buck2obj.h"
#include "lib/obj2buck.h"
#include "lib/lizard.h"
#include "charset/unistream.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

static int verbose;
static struct mempool *pool;
static struct buck2obj_buf *buck_buf;

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
-r\t\tdo not parse V33 buckets, but print the raw content\n\
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

static inline void
dump_oattr(struct fastbuf *out, struct oattr *oa)
{
  for (struct oattr *a = oa; a; a = a->same)
    bprintf(out, "%c%s\n", a->attr, a->val);
}

static void
dump_parsed_bucket(struct fastbuf *out, struct obuck_header *h, struct fastbuf *b)
{
  mp_flush(pool);
  struct odes *o = obj_read_bucket(buck_buf, h->type, h->length, b, NULL);
  if (!o)
    bprintf(out, "Cannot parse bucket %x of type %x and length %d: %m\n", h->oid, h->type, h->length);
  else
  {
    if (h->type < BUCKET_TYPE_V30)
    {
      for (struct oattr *oa = o->attrs; oa; oa = oa->next)
	dump_oattr(out, oa);
    }
    else
    {
#define	IS_HEADER(x) (x=='O' || x=='U')
      for (struct oattr *oa = o->attrs; oa; oa = oa->next)
	if (IS_HEADER(oa->attr))
	  dump_oattr(out, oa);
      bputc(out, '\n');
      for (struct oattr *oa = o->attrs; oa; oa = oa->next)
	if (!IS_HEADER(oa->attr))
	  dump_oattr(out, oa);
    }
  }
}

static void
extract(char *id)
{
  struct fastbuf *b, *out;
  byte buf[1024];
  int l;
  struct obuck_header h;

  h.oid = parse_id(id);
  obuck_init(0);
  obuck_find_by_oid(&h);
  out = bfdopen_shared(1, 65536);
  b = obuck_fetch();
  if (h.type < BUCKET_TYPE_V33 || !buck_buf)
  {
    while ((l = bread(b, buf, sizeof(buf))))
      bwrite(out, buf, l);
  }
  else
    dump_parsed_bucket(out, &h, b);
  bclose(b);
  bclose(out);
  obuck_cleanup();
}

#define	GBUF_TYPE	byte
#define	GBUF_PREFIX(x)	bb_##x
#include "lib/gbuf.h"

static void
insert(byte *arg)
{
  struct fastbuf *b, *in;
  byte buf[4096];
  struct obuck_header h;
  byte *e;
  u32 type;
  bb_t lizard_buf, compressed_buf;

  bb_init(&lizard_buf);
  bb_init(&compressed_buf);
  if (!arg)
    type = BUCKET_TYPE_PLAIN;
  else if (sscanf(arg, "%x", &type) != 1)
    die("Type `%s' is not a hexadecimal number");

  in = bfdopen_shared(0, 4096);
  obuck_init(1);
  do
    {
      uns lizard_filled = 0;
      uns in_body = 0;
      b = NULL;
      while ((e = bgets(in, buf, sizeof(buf))))
	{
	  if (!buf[0])
	  {
	    if (in_body || type < BUCKET_TYPE_V30)
	      break;
	    in_body = 1;
	  }
	  if (!b)
	    b = obuck_create(type);
	  if (type < BUCKET_TYPE_V33)
	  {
	    *e++ = '\n';
	    bwrite(b, buf, e-buf);
	  }
	  else if (in_body == 1)
	  {
	    bputc(b, 0);
	    in_body = 2;
	  }
	  else if (type == BUCKET_TYPE_V33 || !in_body)
	  {
	    bwrite_v33(b, buf[0], buf+1, e-buf-1);
	  }
	  else
	  {
	    uns want_len = lizard_filled + (e-buf) + 6 + LIZARD_NEEDS_CHARS;	// +6 is the maximum UTF-8 length
	    bb_grow(&lizard_buf, want_len);
	    byte *ptr = lizard_buf.ptr + lizard_filled;
	    WRITE_V33(ptr, buf[0], buf+1, e-buf-1);
	    lizard_filled = ptr - lizard_buf.ptr;
	  }
	}
      if (in_body && type == BUCKET_TYPE_V33_LIZARD)
      {
	bputl(b, lizard_filled
#if 0	//TEST error resilience: write wrong length
	    +1
#endif
	    );
	uns want_len = lizard_filled * LIZARD_MAX_MULTIPLY + LIZARD_MAX_ADD;
	bb_grow(&compressed_buf, want_len);
	want_len = lizard_compress(lizard_buf.ptr, lizard_filled, compressed_buf.ptr);
#if 0	//TEST error resilience: tamper the compressed data by removing EOF
	compressed_buf[want_len-1] = 1;
#endif
	bwrite(b, compressed_buf.ptr, want_len);
      }
      if (b)
	{
	  obuck_create_end(b, &h);
	  printf("%08x %d %08x\n", h.oid, h.length, h.type);
	}
    }
  while (e);
  bb_done(&lizard_buf);
  bb_done(&compressed_buf);
  obuck_cleanup();
  bclose(in);
}

static void
cat(void)
{
  struct obuck_header h;
  struct fastbuf *b, *out;
  byte buf[1024];

  obuck_init(0);
  out = bfdopen_shared(1, 65536);
  while (b = obuck_slurp_pool(&h))
    {
      bprintf(out, "### %08x %6d %08x\n", h.oid, h.length, h.type);
      if (h.type < BUCKET_TYPE_V33 || !buck_buf)
      {
	int lf = 1, l;
	while ((l = bread(b, buf, sizeof(buf))))
	{
	  bwrite(out, buf, l);
	  lf = (buf[l-1] == '\n');
	}
	if (!lf)
	  bprintf(out, "\n# <missing EOL>\n");
      }
      else
	dump_parsed_bucket(out, &h, b);
    }
  bclose(out);
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
  uns raw = 0;

  log_init(NULL);
  op = 0;
  while ((i = cf_getopt(argc, argv, CF_SHORT_OPTS "lLd:x:i::cfFqsvr", CF_NO_LONG_OPTS, NULL)) != -1)
    if (i == '?' || op)
      help();
    else if (i == 'v')
      verbose++;
    else if (i == 'r')
      raw++;
    else
      {
	op = i;
	arg = optarg;
      }
  if (optind < argc)
    help();

  if (!raw)
  {
    pool = mp_new(1<<14);
    buck_buf = buck2obj_alloc(pool);
  }
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
  if (buck_buf)
  {
    buck2obj_free(buck_buf);
    mp_delete(pool);
  }

  return 0;
}
