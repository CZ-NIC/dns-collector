/*
 *	Sherlock Library -- Object Buckets
 *
 *	(c) 2001--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/bucket.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"
#include "lib/conf.h"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

static int obuck_fd;
static unsigned int obuck_remains, obuck_check_pad;
static struct fastbuf *obuck_fb;
static struct obuck_header obuck_hdr;
static sh_off_t bucket_start, bucket_current;

/*** Configuration ***/

byte *obuck_name = "not/configured";
static uns obuck_io_buflen = 65536;
static int obuck_shake_buflen = 1048576;
static uns obuck_slurp_buflen = 65536;

static struct cfitem obuck_config[] = {
  { "Buckets",		CT_SECTION,	NULL },
  { "BucketFile",	CT_STRING,	&obuck_name },
  { "BufSize",		CT_INT,		&obuck_io_buflen },
  { "ShakeBufSize",	CT_INT,		&obuck_shake_buflen },
  { "SlurpBufSize",	CT_INT,		&obuck_slurp_buflen },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR obuck_init_config(void)
{
  cf_register(obuck_config);
}

/*** Internal operations ***/

static void
obuck_broken(char *msg)
{
  die("Object pool corrupted: %s (pos=%Lx)", msg, (long long) bucket_start);
}

/*
 *  Unfortunately we cannot use flock() here since it happily permits
 *  locking a shared fd (e.g., after fork()) multiple times. The fcntl
 *  locks are very ugly and they don't support 64-bit offsets, but we
 *  can work around the problem by always locking the first header
 *  in the file.
 */

static inline void
obuck_do_lock(int type)
{
  struct flock fl;

  fl.l_type = type;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = sizeof(struct obuck_header);
  if (fcntl(obuck_fd, F_SETLKW, &fl) < 0)
    die("fcntl lock: %m");
}

inline void
obuck_lock_read(void)
{
  obuck_do_lock(F_RDLCK);
}

inline void
obuck_lock_write(void)
{
  obuck_do_lock(F_WRLCK);
}

inline void
obuck_unlock(void)
{
  obuck_do_lock(F_UNLCK);
}

/*** FastIO emulation ***/

/* We need to use pread/pwrite since we work on fd's shared between processes */

static int
obuck_fb_refill(struct fastbuf *f)
{
  unsigned limit = (obuck_io_buflen < obuck_remains) ? obuck_io_buflen : obuck_remains;
  unsigned size = (limit == obuck_remains) ? (limit+obuck_check_pad+4) : limit;
  int l;

  if (!limit)
    return 0;
  l = sh_pread(obuck_fd, f->buffer, size, bucket_current);
  if (l < 0)
    die("Error reading bucket: %m");
  if ((unsigned) l != size)
    obuck_broken("Short read");
  f->bptr = f->buffer;
  f->bstop = f->buffer + limit;
  bucket_current += limit;
  f->pos = bucket_current - bucket_start - sizeof(obuck_hdr);
  obuck_remains -= limit;
  if (!obuck_remains)	/* Should check the trailer */
    {
      if (GET_U32(f->buffer + size - 4) != OBUCK_TRAILER)
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
      int z = sh_pwrite(obuck_fd, c, l, bucket_current);
      if (z <= 0)
	die("Error writing bucket: %m");
      bucket_current += z;
      l -= z;
      c += z;
    }
  f->bptr = f->buffer;
  f->pos = bucket_current - bucket_start - sizeof(obuck_hdr);
}

/*** Exported functions ***/

void
obuck_init(int writeable)
{
  struct fastbuf *b;
  sh_off_t size;

  obuck_fd = sh_open(obuck_name, (writeable ? O_RDWR | O_CREAT : O_RDONLY), 0666);
  if (obuck_fd < 0)
    die("Unable to open bucket file %s: %m", obuck_name);
  obuck_fb = b = xmalloc_zero(sizeof(struct fastbuf) + obuck_io_buflen + OBUCK_ALIGN + 4);
  b->buffer = (char *)(b+1);
  b->bptr = b->bstop = b->buffer;
  b->bufend = b->buffer + obuck_io_buflen;
  b->name = "bucket";
  b->refill = obuck_fb_refill;
  b->spout = obuck_fb_spout;
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
  close(obuck_fd);
  xfree(obuck_fb);
}

void
obuck_sync(void)
{
  bflush(obuck_fb);
  fsync(obuck_fd);
}

static void
obuck_get(oid_t oid)
{
  struct fastbuf *b = obuck_fb;

  bucket_start = obuck_get_pos(oid);
  bflush(b);
  if (sh_pread(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_start) != sizeof(obuck_hdr))
    obuck_broken("Short header read");
  bucket_current = bucket_start + sizeof(obuck_hdr);
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

  ASSERT(oid < OBUCK_OID_FIRST_SPECIAL);
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
      bucket_current = bucket_start + sizeof(obuck_hdr);
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
  obuck_fb->pos = 0;
  obuck_remains = obuck_hdr.length;
  obuck_check_pad = (OBUCK_ALIGN - sizeof(obuck_hdr) - obuck_hdr.length - 4) & (OBUCK_ALIGN - 1);
  return obuck_fb;
}

void
obuck_fetch_end(struct fastbuf *b UNUSED)
{
}

oid_t
obuck_predict_last_oid(void)
{
  sh_off_t size = sh_seek(obuck_fd, 0, SEEK_END);
  return size >> OBUCK_SHIFT;
}

struct fastbuf *
obuck_create(u32 type)
{
  obuck_lock_write();
  bflush(obuck_fb);
  bucket_start = sh_seek(obuck_fd, 0, SEEK_END);
  if (bucket_start & (OBUCK_ALIGN - 1))
    obuck_broken("Misaligned file");
  obuck_hdr.magic = OBUCK_INCOMPLETE_MAGIC;
  obuck_hdr.oid = bucket_start >> OBUCK_SHIFT;
  obuck_hdr.length = 0;
  obuck_hdr.type = type;
  bucket_current = bucket_start;
  bwrite(obuck_fb, &obuck_hdr, sizeof(obuck_hdr));
  obuck_fb->pos = -sizeof(obuck_hdr);
  return obuck_fb;
}

void
obuck_create_end(struct fastbuf *b UNUSED, struct obuck_header *hdrp)
{
  int pad;
  obuck_hdr.magic = OBUCK_MAGIC;
  obuck_hdr.length = btell(obuck_fb);
  pad = (OBUCK_ALIGN - sizeof(obuck_hdr) - obuck_hdr.length - 4) & (OBUCK_ALIGN - 1);
  while (pad--)
    bputc(obuck_fb, 0);
  bputl(obuck_fb, OBUCK_TRAILER);
  bflush(obuck_fb);
  ASSERT(!(bucket_current & (OBUCK_ALIGN - 1)));
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

/*** Fast reading of the whole pool ***/

static struct fastbuf *obuck_rpf;

static int
obuck_slurp_refill(struct fastbuf *f)
{
  uns l;

  if (!obuck_remains)
    return 0;
  l = bdirect_read_prepare(obuck_rpf, &f->buffer);
  if (!l)
    obuck_broken("Incomplete object");
  l = MIN(l, obuck_remains);
  bdirect_read_commit(obuck_rpf, f->buffer + l);
  obuck_remains -= l;
  f->bptr = f->buffer;
  f->bufend = f->bstop = f->buffer + l;
  return 1;
}

struct fastbuf *
obuck_slurp_pool(struct obuck_header *hdrp)
{
  static struct fastbuf limiter;
  uns l;

  do
    {
      if (!obuck_rpf)
	{
	  obuck_lock_read();
	  obuck_rpf = bopen(obuck_name, O_RDONLY, obuck_slurp_buflen);
	}
      else
	{
	  bsetpos(obuck_rpf, bucket_current - 4);
	  if (bgetl(obuck_rpf) != OBUCK_TRAILER)
	    obuck_broken("Missing trailer");
	}
      bucket_start = btell(obuck_rpf);
      l = bread(obuck_rpf, hdrp, sizeof(struct obuck_header));
      if (!l)
	{
	  bclose(obuck_rpf);
	  obuck_rpf = NULL;
	  obuck_unlock();
	  return NULL;
	}
      if (l != sizeof(struct obuck_header))
	obuck_broken("Short header read");
      if (hdrp->magic != OBUCK_MAGIC)
	obuck_broken("Missing magic number");
      bucket_current = (bucket_start + sizeof(obuck_hdr) + hdrp->length +
			4 + OBUCK_ALIGN - 1) & ~((sh_off_t)(OBUCK_ALIGN - 1));
    }
  while (hdrp->oid == OBUCK_OID_DELETED);
  if (obuck_get_pos(hdrp->oid) != bucket_start)
    obuck_broken("Invalid backlink");
  obuck_remains = hdrp->length;
  limiter.bptr = limiter.bstop = limiter.buffer = limiter.bufend = NULL;
  limiter.name = "Bucket";
  limiter.pos = 0;
  limiter.refill = obuck_slurp_refill;
  return &limiter;
}

/*** Shakedown ***/

void
obuck_shakedown(int (*kibitz)(struct obuck_header *old, oid_t new, byte *buck))
{
  byte *rbuf, *wbuf, *msg;
  sh_off_t rstart, wstart, w_bucket_start;
  int roff, woff, rsize, l;
  struct obuck_header *rhdr, *whdr;

  rbuf = xmalloc(obuck_shake_buflen);
  wbuf = xmalloc(obuck_shake_buflen);
  rstart = wstart = 0;
  roff = woff = rsize = 0;

  /* We need to be the only accessor, all the object ID's are becoming invalid */
  obuck_lock_write();

  for(;;)
    {
      bucket_start = rstart + roff;
      w_bucket_start = wstart + woff;
      if (rsize - roff < OBUCK_ALIGN)
	goto reread;
      rhdr = (struct obuck_header *)(rbuf + roff);
      if (rhdr->magic != OBUCK_MAGIC ||
	  rhdr->oid != OBUCK_OID_DELETED && rhdr->oid != (oid_t)(bucket_start >> OBUCK_SHIFT))
	{
	  msg = "header mismatch";
	  goto broken;
	}
      l = (sizeof(struct obuck_header) + rhdr->length + 4 + OBUCK_ALIGN - 1) & ~(OBUCK_ALIGN-1);
      if (l > obuck_shake_buflen)
	{
	  if (rhdr->oid != OBUCK_OID_DELETED)
	    {
	      msg = "bucket longer than ShakeBufSize";
	      goto broken;
	    }
	  rstart = bucket_start + l;
	  roff = 0;
	  rsize = 0;
	  goto reread;
	}
      if (rsize - roff < l)
	goto reread;
      if (GET_U32(rbuf + roff + l - 4) != OBUCK_TRAILER)
	{
	  msg = "missing trailer";
	  goto broken;
	}
      if (rhdr->oid != OBUCK_OID_DELETED)
	{
	  if (kibitz(rhdr, w_bucket_start >> OBUCK_SHIFT, (byte *)(rhdr+1)))
	    {
	      if (bucket_start == w_bucket_start)
		{
		  /* No copying needed now nor ever in the past, hence woff==0 */
		  wstart += l;
		}
	      else
		{
		  if (obuck_shake_buflen - woff < l)
		    {
		      if (sh_pwrite(obuck_fd, wbuf, woff, wstart) != woff)
			die("obuck_shakedown write failed: %m");
		      wstart += woff;
		      woff = 0;
		    }
		  whdr = (struct obuck_header *)(wbuf+woff);
		  memcpy(whdr, rhdr, l);
		  whdr->oid = w_bucket_start >> OBUCK_SHIFT;
		  woff += l;
		}
	    }
	}
      else
	kibitz(rhdr, OBUCK_OID_DELETED, NULL);
      roff += l;
      continue;

    reread:
      if (roff)
	{
	  memmove(rbuf, rbuf+roff, rsize-roff);
	  rsize -= roff;
	  rstart += roff;
	  roff = 0;
	}
      l = sh_pread(obuck_fd, rbuf+rsize, obuck_shake_buflen-rsize, rstart+rsize);
      if (l < 0)
	die("obuck_shakedown read error: %m");
      if (!l)
	{
	  if (!rsize)
	    break;
	  msg = "unexpected EOF";
	  goto broken;
	}
      rsize += l;
    }
  if (woff)
    {
      if (sh_pwrite(obuck_fd, wbuf, woff, wstart) != woff)
	die("obuck_shakedown write failed: %m");
      wstart += woff;
    }
  sh_ftruncate(obuck_fd, wstart);

  obuck_unlock();
  xfree(rbuf);
  xfree(wbuf);
  return;

 broken:
  log(L_ERROR, "Error during object pool shakedown: %s (pos=%Ld, id=%x), gathering debris", msg, (long long) bucket_start, (uns)(bucket_start >> OBUCK_SHIFT));
  if (woff)
    {
      sh_pwrite(obuck_fd, wbuf, woff, wstart);
      wstart += woff;
    }
  while (wstart + OBUCK_ALIGN <= bucket_start)
    {
      u32 check = OBUCK_TRAILER;
      obuck_hdr.magic = OBUCK_MAGIC;
      obuck_hdr.oid = OBUCK_OID_DELETED;
      if (bucket_start - wstart < 0x40000000)
	obuck_hdr.length = bucket_start - wstart - sizeof(obuck_hdr) - 4;
      else
	obuck_hdr.length = 0x40000000 - sizeof(obuck_hdr) - 4;
      sh_pwrite(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), wstart);
      wstart += sizeof(obuck_hdr) + obuck_hdr.length + 4;
      sh_pwrite(obuck_fd, &check, 4, wstart-4);
    }
  die("Fatal error during object pool shakedown");
}

/*** Testing ***/

#ifdef TEST

#define COUNT 5000
#define MAXLEN 10000
#define KILLPERC 13
#define LEN(i) ((259309*(i))%MAXLEN)

int main(int argc, char **argv)
{
  int ids[COUNT];
  unsigned int i, j, cnt;
  struct obuck_header h;
  struct fastbuf *b;

  log_init(NULL);
  if (cf_getopt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) >= 0 ||
      optind < argc)
  {
    fputs("This program supports only the following command-line arguments:\n" CF_USAGE, stderr);
    exit(1);
  }

  unlink(obuck_name);
  obuck_init(1);
  for(j=0; j<COUNT; j++)
    {
      b = obuck_create();
      for(i=0; i<LEN(j); i++)
        bputc(b, (i+j) % 256);
      obuck_create_end(b, &h);
      printf("Writing %08x %d\n", h.oid, h.length);
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
	printf("Reading %08x %d\n", h.oid, h.length);
	if (h.length != LEN(j))
	  die("Invalid length");
	for(i=0; i<h.length; i++)
	  if ((unsigned) bgetc(b) != (i+j) % 256)
	    die("Contents mismatch");
	if (bgetc(b) != EOF)
	  die("EOF mismatch");
	obuck_fetch_end(b);
      }
  if (obuck_find_first(&h, 0))
    do
      {
	printf("<<< %08x\t%d\n", h.oid, h.length);
	cnt--;
      }
    while (obuck_find_next(&h, 0));
  if (cnt)
    die("Walk mismatch");
  obuck_cleanup();
  return 0;
}

#endif
