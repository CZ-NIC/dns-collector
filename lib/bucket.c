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
static struct obuck_header obuck_hdr, obuck_create_hdr;
static sh_off_t bucket_find_pos;
static struct fastbuf *obuck_write_fb;

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
obuck_broken(char *msg, sh_off_t pos)
{
  die("Object pool corrupted: %s (pos=%Lx)", msg, (long long) pos);
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

struct fb_bucket {
  struct fastbuf fb;
  sh_off_t start_pos;
  uns bucket_size;
  byte buffer[0];
};
#define FB_BUCKET(f) ((struct fb_bucket *)(f)->is_fastbuf)

static int obuck_fb_count;

static void
obuck_fb_close(struct fastbuf *f)
{
  obuck_fb_count--;
  xfree(f);
}

/* We need to use pread/pwrite since we work on fd's shared between processes */

static int
obuck_fb_refill(struct fastbuf *f)
{
  uns remains, bufsize, size, datasize;

  remains = FB_BUCKET(f)->bucket_size - (uns)f->pos;
  bufsize = f->bufend - f->buffer;
  if (!remains)
    return 0;
  sh_off_t start = FB_BUCKET(f)->start_pos;
  sh_off_t pos = start + sizeof(struct obuck_header) + f->pos;
  if (remains <= bufsize)
    {
      datasize = remains;
      size = start + ALIGN(FB_BUCKET(f)->bucket_size + sizeof(struct obuck_header) + 4, OBUCK_ALIGN) - pos;
    }
  else
    size = datasize = bufsize;
  int l = sh_pread(obuck_fd, f->buffer, size, pos);
  if (l < 0)
    die("Error reading bucket: %m");
  if ((unsigned) l != size)
    obuck_broken("Short read", FB_BUCKET(f)->start_pos);
  f->bptr = f->buffer;
  f->bstop = f->buffer + datasize;
  f->pos += datasize;
  if (datasize < size)
    {
      if (GET_U32(f->buffer + size - 4) != OBUCK_TRAILER)
	obuck_broken("Missing trailer", FB_BUCKET(f)->start_pos);
    }
  return datasize;
}

static void
obuck_fb_spout(struct fastbuf *f)
{
  int l = f->bptr - f->buffer;
  char *c = f->buffer;

  while (l)
    {
      int z = sh_pwrite(obuck_fd, c, l, FB_BUCKET(f)->start_pos + sizeof(struct obuck_header) + f->pos);
      if (z <= 0)
	die("Error writing bucket: %m");
      f->pos += z;
      l -= z;
      c += z;
    }
  f->bptr = f->buffer;
}

/*** Exported functions ***/

void
obuck_init(int writeable)
{
  sh_off_t size;

  obuck_fd = sh_open(obuck_name, (writeable ? O_RDWR | O_CREAT : O_RDONLY), 0666);
  if (obuck_fd < 0)
    die("Unable to open bucket file %s: %m", obuck_name);
  obuck_lock_read();
  size = sh_seek(obuck_fd, 0, SEEK_END);
  if (size)
    {
      /* If the bucket pool is not empty, check consistency of its end */
      u32 check;
      if (sh_pread(obuck_fd, &check, 4, size-4) != 4 ||
	  check != OBUCK_TRAILER)
	obuck_broken("Missing trailer of last object", size - 4);
    }
  obuck_unlock();
}

void
obuck_cleanup(void)
{
  close(obuck_fd);
  if (obuck_fb_count)
    log(L_ERROR, "Bug: Unbalanced bucket opens/closes: %d streams remain", obuck_fb_count);
  if (obuck_write_fb)
    log(L_ERROR, "Bug: Forgot to close bucket write stream");
}

void
obuck_sync(void)
{
  if (obuck_write_fb)
    bflush(obuck_write_fb);
  fsync(obuck_fd);
}

static void
obuck_get(oid_t oid)
{
  bucket_find_pos = obuck_get_pos(oid);
  if (sh_pread(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_find_pos) != sizeof(obuck_hdr))
    obuck_broken("Short header read", bucket_find_pos);
  if (obuck_hdr.magic != OBUCK_MAGIC)
    obuck_broken("Missing magic number", bucket_find_pos);
  if (obuck_hdr.oid == OBUCK_OID_DELETED)
    obuck_broken("Access to deleted bucket", bucket_find_pos);
  if (obuck_hdr.oid != oid)
    obuck_broken("Invalid backlink", bucket_find_pos);
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
  bucket_find_pos = 0;
  obuck_hdr.magic = 0;
  return obuck_find_next(hdrp, full);
}

int
obuck_find_next(struct obuck_header *hdrp, int full)
{
  int c;

  for(;;)
    {
      if (obuck_hdr.magic)
	bucket_find_pos = (bucket_find_pos + sizeof(obuck_hdr) + obuck_hdr.length +
			   4 + OBUCK_ALIGN - 1) & ~((sh_off_t)(OBUCK_ALIGN - 1));
      obuck_lock_read();
      c = sh_pread(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_find_pos);
      obuck_unlock();
      if (!c)
	return 0;
      if (c != sizeof(obuck_hdr))
	obuck_broken("Short header read", bucket_find_pos);
      if (obuck_hdr.magic != OBUCK_MAGIC)
	obuck_broken("Missing magic number", bucket_find_pos);
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
  struct fastbuf *b;
  uns official_buflen = ALIGN(MIN(obuck_hdr.length, obuck_io_buflen), OBUCK_ALIGN);
  uns real_buflen = official_buflen + OBUCK_ALIGN;

  b = xmalloc(sizeof(struct fb_bucket) + real_buflen);
  b->buffer = b->bptr = b->bstop = FB_BUCKET(b)->buffer;
  b->bufend = b->buffer + official_buflen;
  b->name = "bucket-read";
  b->pos = 0;
  b->refill = obuck_fb_refill;
  b->spout = NULL;
  b->seek = NULL;
  b->close = obuck_fb_close;
  b->config = NULL;
  FB_BUCKET(b)->start_pos = bucket_find_pos;
  FB_BUCKET(b)->bucket_size = obuck_hdr.length;
  obuck_fb_count++;
  return b;
}

oid_t
obuck_predict_last_oid(void)
{
  /* BEWARE: This is not fork-safe. */
  sh_off_t size = sh_seek(obuck_fd, 0, SEEK_END);
  return size >> OBUCK_SHIFT;
}

struct fastbuf *
obuck_create(u32 type)
{
  ASSERT(!obuck_write_fb);

  obuck_lock_write();
  sh_off_t start = sh_seek(obuck_fd, 0, SEEK_END);
  if (start & (OBUCK_ALIGN - 1))
    obuck_broken("Misaligned file", start);
  obuck_create_hdr.magic = OBUCK_INCOMPLETE_MAGIC;
  obuck_create_hdr.oid = start >> OBUCK_SHIFT;
  obuck_create_hdr.length = 0;
  obuck_create_hdr.type = type;

  struct fastbuf *b = xmalloc(sizeof(struct fb_bucket) + obuck_io_buflen);
  obuck_write_fb = b;
  b->buffer = FB_BUCKET(b)->buffer;
  b->bptr = b->bstop = b->buffer;
  b->bufend = b->buffer + obuck_io_buflen;
  b->pos = -(int)sizeof(obuck_create_hdr);
  b->name = "bucket-write";
  b->refill = NULL;
  b->spout = obuck_fb_spout;
  b->seek = NULL;
  b->close = NULL;
  b->config = NULL;
  FB_BUCKET(b)->start_pos = start;
  FB_BUCKET(b)->bucket_size = 0;
  bwrite(b, &obuck_create_hdr, sizeof(obuck_create_hdr));

  return b;
}

void
obuck_create_end(struct fastbuf *b, struct obuck_header *hdrp)
{
  ASSERT(b == obuck_write_fb);
  obuck_write_fb = NULL;

  obuck_create_hdr.magic = OBUCK_MAGIC;
  obuck_create_hdr.length = btell(b);
  int pad = (OBUCK_ALIGN - sizeof(obuck_create_hdr) - obuck_create_hdr.length - 4) & (OBUCK_ALIGN - 1);
  while (pad--)
    bputc(b, 0);
  bputl(b, OBUCK_TRAILER);
  bflush(b);
  ASSERT(!((FB_BUCKET(b)->start_pos + sizeof(obuck_create_hdr) + b->pos) & (OBUCK_ALIGN - 1)));
  if (sh_pwrite(obuck_fd, &obuck_create_hdr, sizeof(obuck_create_hdr), FB_BUCKET(b)->start_pos) != sizeof(obuck_create_hdr))
    die("Bucket header update failed: %m");
  obuck_unlock();
  memcpy(hdrp, &obuck_create_hdr, sizeof(obuck_create_hdr));
  xfree(b);
}

void
obuck_delete(oid_t oid)
{
  obuck_lock_write();
  obuck_get(oid);
  obuck_hdr.oid = OBUCK_OID_DELETED;
  sh_pwrite(obuck_fd, &obuck_hdr, sizeof(obuck_hdr), bucket_find_pos);
  obuck_unlock();
}

/*** Fast reading of the whole pool ***/

static struct fastbuf *obuck_rpf;
static uns slurp_remains;
static sh_off_t slurp_start, slurp_current;

static int
obuck_slurp_refill(struct fastbuf *f)
{
  uns l;

  if (!slurp_remains)
    return 0;
  l = bdirect_read_prepare(obuck_rpf, &f->buffer);
  if (!l)
    obuck_broken("Incomplete object", slurp_start);
  l = MIN(l, slurp_remains);
  bdirect_read_commit(obuck_rpf, f->buffer + l);
  slurp_remains -= l;
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
	  bsetpos(obuck_rpf, slurp_current - 4);
	  if (bgetl(obuck_rpf) != OBUCK_TRAILER)
	    obuck_broken("Missing trailer", slurp_start);
	}
      slurp_start = btell(obuck_rpf);
      l = bread(obuck_rpf, hdrp, sizeof(struct obuck_header));
      if (!l)
	{
	  bclose(obuck_rpf);
	  obuck_rpf = NULL;
	  obuck_unlock();
	  return NULL;
	}
      if (l != sizeof(struct obuck_header))
	obuck_broken("Short header read", slurp_start);
      if (hdrp->magic != OBUCK_MAGIC)
	obuck_broken("Missing magic number", slurp_start);
      slurp_current = (slurp_start + sizeof(obuck_hdr) + hdrp->length +
		       4 + OBUCK_ALIGN - 1) & ~((sh_off_t)(OBUCK_ALIGN - 1));
    }
  while (hdrp->oid == OBUCK_OID_DELETED);
  if (obuck_get_pos(hdrp->oid) != slurp_start)
    obuck_broken("Invalid backlink", slurp_start);
  slurp_remains = hdrp->length;
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
  sh_off_t rstart, wstart, r_bucket_start, w_bucket_start;
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
      r_bucket_start = rstart + roff;
      w_bucket_start = wstart + woff;
      if (rsize - roff < OBUCK_ALIGN)
	goto reread;
      rhdr = (struct obuck_header *)(rbuf + roff);
      if (rhdr->magic != OBUCK_MAGIC ||
	  rhdr->oid != OBUCK_OID_DELETED && rhdr->oid != (oid_t)(r_bucket_start >> OBUCK_SHIFT))
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
	  rstart = r_bucket_start + l;
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
	      if (r_bucket_start == w_bucket_start)
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
  log(L_ERROR, "Error during object pool shakedown: %s (pos=%Ld, id=%x), gathering debris", msg, (long long) r_bucket_start, (uns)(r_bucket_start >> OBUCK_SHIFT));
  if (woff)
    {
      sh_pwrite(obuck_fd, wbuf, woff, wstart);
      wstart += woff;
    }
  while (wstart + OBUCK_ALIGN <= r_bucket_start)
    {
      u32 check = OBUCK_TRAILER;
      obuck_hdr.magic = OBUCK_MAGIC;
      obuck_hdr.oid = OBUCK_OID_DELETED;
      if (r_bucket_start - wstart < 0x40000000)
	obuck_hdr.length = r_bucket_start - wstart - sizeof(obuck_hdr) - 4;
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
      b = obuck_create(BUCKET_TYPE_PLAIN);
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
	bclose(b);
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
