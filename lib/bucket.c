/*
 *	Sherlock Library -- Object Buckets
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/bucket.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

static int obuck_fd;
static unsigned int obuck_remains, obuck_check_pad;
static struct fastbuf *obuck_fb;
static struct obuck_header obuck_hdr;
static sh_off_t bucket_start;
byte *obuck_name = "db/objects";		/* FIXME */

/*** Internal operations ***/

static void
obuck_broken(char *msg)
{
  die("Object pool corrupted: %s (pos=%Lx)", msg, (long long) bucket_start);	/* FIXME */
}

static inline void
obuck_lock_read(void)
{
  flock(obuck_fd, LOCK_SH);
}

static inline void
obuck_lock_write(void)
{
  flock(obuck_fd, LOCK_EX);
}

static inline void
obuck_unlock(void)
{
  flock(obuck_fd, LOCK_UN);
}

/*** FastIO emulation ***/

/* We need to use pread/pwrite since we work on fd's shared between processes */

static int
obuck_fb_refill(struct fastbuf *f)
{
  unsigned limit = (f->buflen < obuck_remains) ? f->buflen : obuck_remains;
  unsigned size = (limit == obuck_remains) ? (limit+obuck_check_pad+4) : limit;
  int l;

  if (!limit)
    return 0;
  l = sh_pread(f->fd, f->buffer, size, f->fdpos);
  if (l < 0)
    die("Error reading bucket: %m");
  if ((unsigned) l != size)
    obuck_broken("Short read");
  f->bptr = f->buffer;
  f->bstop = f->buffer + limit;
  f->pos = f->fdpos;
  f->fdpos += limit;
  obuck_remains -= limit;
  if (!obuck_remains)	/* Should check the trailer */
    {
      u32 check;
      memcpy(&check, f->buffer + size - 4, 4);
      if (check != OBUCK_TRAILER)
	obuck_broken("Missing trailer");
    }
  return limit;
}

static void
obuck_fb_spout(struct fastbuf *f)
{
  int l = f->bptr - f->buffer;
  char *c = f->buffer;

  while (l)
    {
      int z = sh_pwrite(f->fd, c, l, f->fdpos);
      if (z <= 0)
	die("Error writing bucket: %m");
      f->fdpos += z;
      l -= z;
      c += z;
    }
  f->bptr = f->buffer;
  f->pos = f->fdpos;
}

static void
obuck_fb_close(struct fastbuf *f)
{
  close(f->fd);
}

/*** Exported functions ***/

void
obuck_init(int writeable)
{
  struct fastbuf *b;
  int buflen = 65536;
  sh_off_t size;

  obuck_fd = open(obuck_name, (writeable ? O_RDWR | O_CREAT : O_RDONLY), 0666);
  obuck_fb = b = xmalloc(sizeof(struct fastbuf) + buflen + OBUCK_ALIGN + 4);
  bzero(b, sizeof(struct fastbuf));
  b->buflen = buflen;
  b->buffer = (char *)(b+1);
  b->bptr = b->bstop = b->buffer;
  b->bufend = b->buffer + buflen;
  b->name = "bucket";
  b->fd = obuck_fd;
  b->refill = obuck_fb_refill;
  b->spout = obuck_fb_spout;
  b->close = obuck_fb_close;
  obuck_lock_read();
  size = sh_seek(obuck_fd, 0, SEEK_END);
  if (size)
    {
      /* If the bucket pool is not empty, check consistency of its end */
      u32 check;
      bucket_start = size - 4;	/* for error reporting */
      if (sh_pread(obuck_fd, &check, 4, size-4) != 4 ||
	  check != OBUCK_TRAILER)
	obuck_broken("Missing trailer of last object");
    }
  obuck_unlock();
}

void
obuck_cleanup(void)
{
  bclose(obuck_fb);
}

void					/* FIXME: Call somewhere :) */
obuck_sync(void)
{
  bflush(obuck_fb);
  fsync(obuck_fd);
}

static void
obuck_get(oid_t oid)
{
  struct fastbuf *b = obuck_fb;

  bucket_start = ((sh_off_t) oid) << OBUCK_SHIFT;
  bflush(b);
  if (sh_pread(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_start) != sizeof(obuck_hdr))
    obuck_broken("Short header read");
  b->fdpos = bucket_start + sizeof(obuck_hdr);
  if (obuck_hdr.magic != OBUCK_MAGIC)
    obuck_broken("Missing magic number");
  if (obuck_hdr.oid == OBUCK_OID_DELETED)
    obuck_broken("Access to deleted bucket");
  if (obuck_hdr.oid != oid)
    obuck_broken("Invalid backlink");
}

void
obuck_find_by_oid(struct obuck_header *hdrp)
{
  oid_t oid = hdrp->oid;

  obuck_lock_read();
  obuck_get(oid);
  obuck_unlock();
  memcpy(hdrp, &obuck_hdr, sizeof(obuck_hdr));
}

int
obuck_find_first(struct obuck_header *hdrp, int full)
{
  bucket_start = 0;
  obuck_hdr.magic = 0;
  return obuck_find_next(hdrp, full);
}

int
obuck_find_next(struct obuck_header *hdrp, int full)
{
  int c;
  struct fastbuf *b = obuck_fb;

  for(;;)
    {
      if (obuck_hdr.magic)
	bucket_start = (bucket_start + sizeof(obuck_hdr) + obuck_hdr.length +
			4 + OBUCK_ALIGN - 1) & ~((sh_off_t)(OBUCK_ALIGN - 1));
      bflush(b);
      obuck_lock_read();
      c = sh_pread(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_start);
      obuck_unlock();
      if (!c)
	return 0;
      if (c != sizeof(obuck_hdr))
	obuck_broken("Short header read");
      b->fdpos = bucket_start + sizeof(obuck_hdr);
      if (obuck_hdr.magic != OBUCK_MAGIC)
	obuck_broken("Missing magic number");
      if (obuck_hdr.oid != OBUCK_OID_DELETED || full)
	{
	  memcpy(hdrp, &obuck_hdr, sizeof(obuck_hdr));
	  return 1;
	}
    }
}

struct fastbuf *
obuck_fetch(void)
{
  obuck_remains = obuck_hdr.length;
  obuck_check_pad = (OBUCK_ALIGN - sizeof(obuck_hdr) - obuck_hdr.length - 4) & (OBUCK_ALIGN - 1);
  return obuck_fb;
}

void
obuck_fetch_end(struct fastbuf *b UNUSED)
{
}

struct fastbuf *
obuck_create(void)
{
  obuck_lock_write();
  bflush(obuck_fb);
  bucket_start = sh_seek(obuck_fd, 0, SEEK_END);
  if (bucket_start & (OBUCK_ALIGN - 1))
    obuck_broken("Misaligned file");
  obuck_hdr.magic = OBUCK_INCOMPLETE_MAGIC;
  obuck_hdr.oid = bucket_start >> OBUCK_SHIFT;
  obuck_hdr.length = obuck_hdr.orig_length = 0;
  obuck_fb->fdpos = obuck_fb->pos = bucket_start;
  bwrite(obuck_fb, &obuck_hdr, sizeof(obuck_hdr));
  return obuck_fb;
}

void
obuck_create_end(struct fastbuf *b UNUSED, struct obuck_header *hdrp)
{
  int pad;
  obuck_hdr.magic = OBUCK_MAGIC;
  obuck_hdr.length = obuck_hdr.orig_length = btell(obuck_fb) - bucket_start - sizeof(obuck_hdr);
  pad = (OBUCK_ALIGN - sizeof(obuck_hdr) - obuck_hdr.length - 4) & (OBUCK_ALIGN - 1);
  while (pad--)
    bputc(obuck_fb, 0);
  bputl(obuck_fb, OBUCK_TRAILER);
  bflush(obuck_fb);
  ASSERT(!(btell(obuck_fb) & (OBUCK_ALIGN - 1)));
  sh_pwrite(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_start);
  obuck_unlock();
  memcpy(hdrp, &obuck_hdr, sizeof(obuck_hdr));
}

void
obuck_delete(oid_t oid)
{
  obuck_lock_write();
  obuck_get(oid);
  obuck_hdr.oid = OBUCK_OID_DELETED;
  sh_pwrite(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_start);
  obuck_unlock();
}

/*** Testing ***/

#ifdef TEST

#define COUNT 100
#define MAXLEN 10000
#define KILLPERC 13
#define LEN(i) ((259309*(i))%MAXLEN)

int main(void)
{
  int ids[COUNT];
  unsigned int i, j, cnt;
  struct obuck_header h;
  struct fastbuf *b;
  unlink(obuck_name);
  obuck_init(1);
  for(j=0; j<COUNT; j++)
    {
      b = obuck_create();
      for(i=0; i<LEN(j); i++)
        bputc(b, (i+j) % 256);
      obuck_create_end(b, &h);
      printf("Writing %08x %d -> %d\n", h.oid, h.orig_length, h.length);
      ids[j] = h.oid;
    }
  for(j=0; j<COUNT; j++)
    if (j % 100 < KILLPERC)
      {
	printf("Deleting %08x\n", ids[j]);
	obuck_delete(ids[j]);
      }
  cnt = 0;
  for(j=0; j<COUNT; j++)
    if (j % 100 >= KILLPERC)
      {
	cnt++;
	h.oid = ids[j];
	obuck_find_by_oid(&h);
	b = obuck_fetch();
	printf("Reading %08x %d -> %d\n", h.oid, h.orig_length, h.length);
	if (h.orig_length != LEN(j))
	  die("Invalid length");
	for(i=0; i<h.orig_length; i++)
	  if ((unsigned) bgetc(b) != (i+j) % 256)
	    die("Contents mismatch");
	if (bgetc(b) != EOF)
	  die("EOF mismatch");
	obuck_fetch_end(b);
      }
  if (obuck_find_first(&h, 0))
    do
      {
	printf("<<< %08x\t%d\n", h.oid, h.orig_length);
	cnt--;
      }
    while (obuck_find_next(&h, 0));
  if (cnt)
    die("Walk mismatch");
  obuck_cleanup();
  return 0;
}

#endif
