/*
 *	Sherlock Library -- Object Buckets
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 *
 *	Warning: Touches internals of the fb-file module!
 */

#include "lib/lib.h"
#include "lib/bucket.h"
#include "lib/fastbuf.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

static int obuck_fd;
static struct fastbuf *obuck_fb;
static struct obuck_header obuck_hdr;
static sh_off_t start_of_this, start_of_next;
static char *obuck_name = "db/objects";		/* FIXME */

void
obuck_init(int writeable)
{
  obuck_fb = bopen(obuck_name, (writeable ? O_RDWR | O_CREAT : O_RDONLY), 65536);
  obuck_fd = obuck_fb->fd;
}

void
obuck_cleanup(void)
{
  bclose(obuck_fb);
}

static void
obuck_broken(char *msg)
{
  die("Object pool corrupted: %s", msg);	/* FIXME */
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

static void
obuck_fetch_header(oid_t oid)
{
  start_of_this = ((sh_off_t) oid) << OBUCK_SHIFT;
  bsetpos(obuck_fb, start_of_this);
  bread(obuck_fb, &obuck_hdr, sizeof(obuck_hdr));
  if (obuck_hdr.magic != OBUCK_MAGIC)
    obuck_broken("Missing magic number");
  if (obuck_hdr.oid == OBUCK_OID_DELETED)
    obuck_broken("Access to deleted bucket");
  if (obuck_hdr.oid != oid)
    obuck_broken("Invalid backlink");
}

struct fastbuf *
obuck_fetch(struct obuck_header *hdrp)
{
  obuck_lock_read();
  obuck_fetch_header(hdrp->oid);
  memcpy(hdrp, &obuck_hdr, sizeof(obuck_hdr));
  return obuck_fb;
}

void
obuck_fetch_abort(struct fastbuf *b UNUSED)
{
  obuck_unlock();
}

void
obuck_fetch_end(struct fastbuf *b UNUSED)
{
  if (bgetl(b) != OBUCK_TRAILER)
    obuck_broken("Corrupted trailer");
  obuck_unlock();
}

struct fastbuf *
obuck_write(void)
{
  obuck_lock_write();
  bseek(obuck_fb, 0, SEEK_END);
  start_of_this = btell(obuck_fb);
  if (start_of_this & (OBUCK_ALIGN - 1))
    obuck_broken("Misaligned file");
  obuck_hdr.magic = 0;
  obuck_hdr.oid = start_of_this >> OBUCK_SHIFT;
  obuck_hdr.length = obuck_hdr.orig_length = 0;
  bwrite(obuck_fb, &obuck_hdr, sizeof(obuck_hdr));
  return obuck_fb;
}

void
obuck_write_end(struct fastbuf *b UNUSED, struct obuck_header *hdrp)
{
  int pad;
  obuck_hdr.magic = OBUCK_MAGIC;
  obuck_hdr.length = obuck_hdr.orig_length = btell(obuck_fb) - start_of_this - sizeof(obuck_hdr);
  bputl(obuck_fb, OBUCK_TRAILER);
  pad = (OBUCK_ALIGN - sizeof(obuck_hdr) - obuck_hdr.length - 4) & (OBUCK_ALIGN - 1);
  while (pad--)
    bputc(obuck_fb, 0);
  bflush(obuck_fb);
  bsetpos(obuck_fb, start_of_this);
  /* FIXME: Can be replaced with single pwrite */
  bwrite(obuck_fb, &obuck_hdr, sizeof(obuck_hdr));
  bflush(obuck_fb);
  obuck_unlock();
  memcpy(hdrp, &obuck_hdr, sizeof(obuck_hdr));
}

void
obuck_delete(oid_t oid)
{
  obuck_lock_write();
  obuck_fetch_header(oid);
  obuck_hdr.oid = OBUCK_OID_DELETED;
  bflush(obuck_fb);
  bsetpos(obuck_fb, start_of_this);
  bwrite(obuck_fb, &obuck_hdr, sizeof(obuck_hdr));
  bflush(obuck_fb);
  obuck_unlock();
}

struct fastbuf *
obuck_walk_init(void)
{
  start_of_this = start_of_next = 0;
  obuck_lock_read();
  return obuck_fb;
}

struct fastbuf *
obuck_walk_next(struct fastbuf *b, struct obuck_header *hdrp)
{
  int c;

restart:
  start_of_this = start_of_next;
  bsetpos(b, start_of_this);
  c = bgetc(b);
  if (c < 0)
    return NULL;
  bungetc(b, c);
  bread(b, &obuck_hdr, sizeof(obuck_hdr));
  if (obuck_hdr.magic != OBUCK_MAGIC)
    obuck_broken("Missing magic number");
  start_of_next = (start_of_this + sizeof(obuck_hdr) + obuck_hdr.orig_length +
  	4 + OBUCK_ALIGN - 1) & ~((sh_off_t)(OBUCK_ALIGN - 1));
  if (obuck_hdr.oid == OBUCK_OID_DELETED)
    goto restart;
  memcpy(hdrp, &obuck_hdr, sizeof(obuck_hdr));
  return b;
}

void
obuck_walk_end(struct fastbuf *b UNUSED)
{
  obuck_unlock();
}

#ifdef TEST
int main(void)
{
  int i, j;
  struct obuck_header h;
  struct fastbuf *b;
  obuck_init(1);
  for(j=0; j<100; j++)
    {
      b = obuck_write();
      for(i=0; i<100*j; i++)
        bputc(b, i);
      obuck_write_end(b, &h);
      printf("%d\t%08x\t%d\n", j, h.oid, h.orig_length);
    }
  obuck_delete(0);
  b = obuck_walk_init();
  while (b = obuck_walk_next(b, &h))
    {
      printf("<<< %08x\t%d\n", h.oid, h.orig_length);
    }
  obuck_walk_end(b);
  obuck_cleanup();
  return 0;
}
#endif
