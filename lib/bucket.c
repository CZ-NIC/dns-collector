/*
 *	Sherlock Library -- Object Buckets
 *
 *	(c) 2001--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

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
#include <alloca.h>

static int obuck_fd;
static struct obuck_header obuck_hdr, obuck_create_hdr;
static sh_off_t bucket_find_pos;
static struct fastbuf *obuck_write_fb;

/*** Configuration ***/

byte *obuck_name = "not/configured";
static uns obuck_io_buflen = 65536;
static int obuck_shake_buflen = 1048576;
static uns obuck_shake_security;
static uns obuck_slurp_buflen = 65536;

static struct cfitem obuck_config[] = {
  { "Buckets",		CT_SECTION,	NULL },
  { "BucketFile",	CT_STRING,	&obuck_name },
  { "BufSize",		CT_INT,		&obuck_io_buflen },
  { "ShakeBufSize",	CT_INT,		&obuck_shake_buflen },
  { "ShakeSecurity",	CT_INT,		&obuck_shake_security },
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
 *  We need several types of locks:
 *
 *	Read lock	reading parts of bucket file
 *	Write lock	any write operations
 *	Append lock	appending to the end of the file
 *	Scan lock	reading parts which we are certain they exist
 *
 *  Multiple read and scan locks can co-exist together.
 *  Scan locks can co-exist with an append lock.
 *  There can be at most one write/append lock at a time.
 *
 *  These lock types map to a pair of normal read-write locks which
 *  we represent as fcntl() locks on the first and second byte of the
 *  bucket file. [We cannot use flock() since it happily permits
 *  locking a shared fd (e.g., after fork()) multiple times at it also
 *  doesn't offer multiple locks on a single file.]
 *
 *			byte0		byte1
 *	Read		<read>		<read>
 *	Write		<write>		<write>
 *	Append		<write>		-
 *	Scan		-		<read>
 */

static inline void
obuck_do_lock(int type, int start, int len)
{
  struct flock fl;

  fl.l_type = type;
  fl.l_whence = SEEK_SET;
  fl.l_start = start;
  fl.l_len = len;
  if (fcntl(obuck_fd, F_SETLKW, &fl) < 0)
    die("fcntl lock: %m");
}

inline void
obuck_lock_read(void)
{
  obuck_do_lock(F_RDLCK, 0, 2);
}

inline void
obuck_lock_write(void)
{
  obuck_do_lock(F_WRLCK, 0, 2);
}

static inline void
obuck_lock_append(void)
{
  obuck_do_lock(F_WRLCK, 0, 1);
}

static inline void
obuck_lock_read_to_scan(void)
{
  obuck_do_lock(F_UNLCK, 0, 1);
}

inline void
obuck_unlock(void)
{
  obuck_do_lock(F_UNLCK, 0, 2);
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
  if (!remains)
    return 0;
  f->buffer = FB_BUCKET(f)->buffer;	/* Could have been trimmed by bdirect_read_commit_modified() */
  bufsize = f->bufend - f->buffer;
  sh_off_t start = FB_BUCKET(f)->start_pos;
  sh_off_t pos = start + sizeof(struct obuck_header) + f->pos;
  if (remains <= bufsize)
    {
      datasize = remains;
      size = start + obuck_bucket_size(FB_BUCKET(f)->bucket_size) - pos;
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
	bucket_find_pos += obuck_bucket_size(obuck_hdr.length);
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
  b->can_overwrite_buffer = 2;
  FB_BUCKET(b)->start_pos = bucket_find_pos;
  FB_BUCKET(b)->bucket_size = obuck_hdr.length;
  obuck_fb_count++;
  return b;
}

oid_t
obuck_predict_last_oid(void)
{
  sh_off_t size = sh_seek(obuck_fd, 0, SEEK_END);
  return (oid_t)(size >> OBUCK_SHIFT);
}

struct fastbuf *
obuck_create(u32 type)
{
  ASSERT(!obuck_write_fb);

  obuck_lock_append();
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
  b->can_overwrite_buffer = 0;
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
static sh_off_t slurp_start, slurp_current, slurp_end;

static int
obuck_slurp_refill(struct fastbuf *f)
{
  if (!slurp_remains)
    return 0;
  uns l = bdirect_read_prepare(obuck_rpf, &f->buffer);
  if (!l)
    obuck_broken("Incomplete object", slurp_start);
  l = MIN(l, slurp_remains);
  /* XXX: This probably should be bdirect_read_commit_modified() in some cases,
   *      but it doesn't hurt since we aren't going to seek.
   */
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
	  slurp_end = bfilesize(obuck_rpf);
	  obuck_lock_read_to_scan();
	}
      else
	{
	  bsetpos(obuck_rpf, slurp_current - 4);
	  if (bgetl(obuck_rpf) != OBUCK_TRAILER)
	    obuck_broken("Missing trailer", slurp_start);
	}
      slurp_start = btell(obuck_rpf);
      if (slurp_start < slurp_end)
	l = bread(obuck_rpf, hdrp, sizeof(struct obuck_header));
      else
	l = 0;
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
      slurp_current = slurp_start + obuck_bucket_size(hdrp->length);
    }
  while (hdrp->oid == OBUCK_OID_DELETED);
  if (obuck_get_pos(hdrp->oid) != slurp_start)
    obuck_broken("Invalid backlink", slurp_start);
  slurp_remains = hdrp->length;
  limiter.bptr = limiter.bstop = limiter.buffer = limiter.bufend = NULL;
  limiter.name = "Bucket";
  limiter.pos = 0;
  limiter.refill = obuck_slurp_refill;
  limiter.can_overwrite_buffer = obuck_rpf->can_overwrite_buffer;
  return &limiter;
}

/*** Shakedown ***/

static inline void
shake_write(void *addr, int len, sh_off_t pos)
{
  int l = sh_pwrite(obuck_fd, addr, len, pos);
  if (l != len)
    {
      if (l < 0)
	die("obuck_shakedown write error: %m");
      else
	die("obuck_shakedown write error: disk full");
    }
}

static inline void
shake_sync(void)
{
  if (obuck_shake_security > 1)
    fdatasync(obuck_fd);
}

static void
shake_write_backup(sh_off_t bpos, byte *norm_buf, int norm_size, byte *fragment, int frag_size, sh_off_t frag_pos, int more_size)
{
  struct obuck_header *bhdr;
  int boff = 0;
  int l;
  oid_t old_oid;

  /* First of all, the "normal" part -- everything that will be written in this pass */
  DBG("Backing up first round of changes at position %Lx + %x", (long long) bpos, norm_size);
  while (boff < norm_size)
    {
      /* This needn't be optimized for speed. */
      bhdr = (struct obuck_header *) (norm_buf + boff);
      ASSERT(bhdr->magic == OBUCK_MAGIC);
      l = obuck_bucket_size(bhdr->length);
      old_oid = bhdr->oid;
      bhdr->oid = bpos >> OBUCK_SHIFT;
      shake_write(bhdr, l, bpos);
      bhdr->oid = old_oid;
      boff += l;
      bpos += l;
    }

  /* If we have an incomplete bucket at the end of the buffer, we must copy it as well. */
  if (more_size)
    {
      DBG("Backing up fragment of size %x and %x more", frag_size, more_size);

      /* First the part we already have in the buffer */
      bhdr = (struct obuck_header *) fragment;
      ASSERT(bhdr->magic == OBUCK_MAGIC);
      old_oid = bhdr->oid;
      bhdr->oid = bpos >> OBUCK_SHIFT;
      shake_write(bhdr, frag_size, bpos);
      bhdr->oid = old_oid;
      bpos += frag_size;

      /* And then the rest, using a small 64K buffer */
      byte *auxbuf = alloca(65536);
      l = 0;
      while (l < more_size)
	{
	  int j = MIN(more_size-l, 65536);
	  if (sh_pread(obuck_fd, auxbuf, j, frag_pos + frag_size + l) != j)
	    die("obuck_shakedown read error: %m");
	  shake_write(auxbuf, j, bpos);
	  bpos += j;
	  l += j;
	}
    }
}

static void
shake_erase(sh_off_t start, sh_off_t end)
{
  if (start > end)
    die("shake_erase called with negative length, that's a bug");
  ASSERT(!(start & (OBUCK_ALIGN-1)) && !(end & (OBUCK_ALIGN-1)));
  while (start < end)
    {
      u32 check = OBUCK_TRAILER;
      obuck_hdr.magic = OBUCK_MAGIC;
      obuck_hdr.oid = OBUCK_OID_DELETED;
      uns len = MIN(0x40000000, end-start);
      obuck_hdr.length = len - sizeof(obuck_hdr) - 4;
      DBG("Erasing %08x bytes at %Lx", len, (long long) start);
      shake_write(&obuck_hdr, sizeof(obuck_hdr), start);
      start += len;
      shake_write(&check, 4, start-4);
    }
}

void
obuck_shakedown(int (*kibitz)(struct obuck_header *old, oid_t new, byte *buck))
{
  byte *buf;						/* Shakedown buffer and its size */
  int buflen = ALIGN(obuck_shake_buflen, OBUCK_ALIGN);
  byte *msg;						/* Error message we will print */
  sh_off_t rstart, wstart;				/* Original and new position of buffer start */
  sh_off_t r_bucket_start, w_bucket_start;		/* Original and new position of the current bucket */
  int roff, woff;					/* Orig/new position of the current bucket relative to buffer start */
  int rsize;						/* Number of original bytes in the buffer */
  int l;						/* Raw size of the current bucket */
  int changed = 0;					/* "Something has been altered" flag */
  int wrote_anything = 0;				/* We already did a write to the bucket file */
  struct obuck_header *rhdr, *whdr;			/* Original and new address of header of the current bucket */
  sh_off_t r_file_size;					/* Original size of the bucket file */
  int more;						/* How much does the last bucket overlap the buffer */

  buf = xmalloc(buflen);
  rstart = wstart = 0;
  roff = woff = rsize = 0;

  /* We need to be the only accessor, all the object ID's are becoming invalid */
  obuck_lock_write();
  r_file_size = sh_seek(obuck_fd, 0, SEEK_END);
  ASSERT(!(r_file_size & (OBUCK_ALIGN - 1)));
  if (r_file_size >= (0x100000000 << OBUCK_SHIFT) - buflen)
    die("Bucket file is too large for safe shakedown. Shaking down with Bucket.ShakeSecurity=0 will still work.");

  DBG("Starting shakedown. Buffer size is %d, original length %Lx", buflen, (long long) r_file_size);

  for(;;)
    {
      r_bucket_start = rstart + roff;
      w_bucket_start = wstart + woff;
      rhdr = (struct obuck_header *)(buf + roff);
      whdr = (struct obuck_header *)(buf + woff);
      if (roff == rsize)
	{
	  more = 0;
	  goto next;
	}
      if (rhdr->magic != OBUCK_MAGIC ||
	  rhdr->oid != OBUCK_OID_DELETED && rhdr->oid != (oid_t)(r_bucket_start >> OBUCK_SHIFT))
	{
	  msg = "header mismatch";
	  goto broken;
	}
      l = obuck_bucket_size(rhdr->length);
      if (l > buflen)
	{
	  if (rhdr->oid != OBUCK_OID_DELETED)
	    {
	      msg = "bucket longer than ShakeBufSize";
	      goto broken;
	    }
	  /* Empty buckets are allowed to be large, but we need to handle them extra */
	  DBG("Tricking around an extra-large empty bucket at %Lx + %x", (long long)r_bucket_start, l);
	  rsize = roff + l;
	}
      else
	{
	  if (rsize - roff < l)
	    {
	      more = l - (rsize - roff);
	      goto next;
	    }
	  if (GET_U32((byte *)rhdr + l - 4) != OBUCK_TRAILER)
	    {
	      msg = "missing trailer";
	      goto broken;
	    }
	}
      if (rhdr->oid != OBUCK_OID_DELETED)
	{
	  int status = kibitz(rhdr, w_bucket_start >> OBUCK_SHIFT, (byte *)(rhdr+1));
	  if (status)
	    {
	      int lnew = l;
	      if (status > 1)
		{
		  /* Changed! Reconstruct the trailer. */
		  lnew = obuck_bucket_size(rhdr->length);
		  ASSERT(lnew <= l);
		  PUT_U32((byte *)rhdr + lnew - 4, OBUCK_TRAILER);
		  changed = 1;
		}
	      whdr = (struct obuck_header *)(buf+woff);
	      if (rhdr != whdr)
		memmove(whdr, rhdr, lnew);
	      whdr->oid = w_bucket_start >> OBUCK_SHIFT;
	      woff += lnew;
	    }
	  else
	    changed = 1;
	}
      else
	{
	  kibitz(rhdr, OBUCK_OID_DELETED, NULL);
	  changed = 1;
	}
      roff += l;
      continue;

    next:
      if (changed)
	{
	  /* Write the new contents of the bucket file */
	  if (!wrote_anything)
	    {
	      if (obuck_shake_security)
		{
		  /* But first write a backup at the end of the file to ensure nothing can be lost. */
		  shake_write_backup(r_file_size, buf, woff, buf+roff, rsize-roff, rstart+roff, more);
		  shake_sync();
		}
	      wrote_anything = 1;
	    }
	  if (woff)
	    {
	      DBG("Write %Lx %x", wstart, woff);
	      shake_write(buf, woff, wstart);
	      shake_sync();
	    }
	}
      else
	ASSERT(wstart == rstart);

      /* In any case, update the write position */
      wstart += woff;
      woff = 0;

      /* Skip what's been read and if there is any fragment at the end of the buffer, move it to the start */
      rstart += roff;
      if (more)
	{
	  memmove(buf, buf+roff, rsize-roff);
	  rsize = rsize-roff;
	}
      else
	rsize = 0;

      /* And refill the buffer */
      r_bucket_start = rstart+rsize;	/* Also needed for error messages */
      l = sh_pread(obuck_fd, buf+rsize, MIN(buflen-rsize, r_file_size - r_bucket_start), r_bucket_start);
      DBG("Read  %Lx %x (%x inherited)", (long long)r_bucket_start, l, rsize);
      if (l < 0)
	die("obuck_shakedown read error: %m");
      if (!l)
	{
	  if (!more)
	    break;
	  msg = "unexpected EOF";
	  goto broken;
	}
      if (l & (OBUCK_ALIGN-1))
	{
	  msg = "garbage at the end of file";
	  goto broken;
	}
      rsize += l;
      roff = 0;
    }

  DBG("Finished at position %Lx", (long long) wstart);
  sh_ftruncate(obuck_fd, wstart);
  shake_sync();

  obuck_unlock();
  xfree(buf);
  return;

 broken:
  log(L_ERROR, "Error during object pool shakedown: %s (pos=%Ld, id=%x), gathering debris",
      msg, (long long) r_bucket_start, (uns)(r_bucket_start >> OBUCK_SHIFT));
  /*
   * We can attempt to clean up the bucket file by erasing everything between the last
   * byte written and the next byte to be read. If the secure mode is switched on, we can
   * guarantee that no data are lost, only some might be duplicated.
   */
  shake_erase(wstart, rstart);
  die("Fatal error during object pool shakedown");
}

/*** Testing ***/

#ifdef TEST

#define COUNT 5000
#define MAXLEN 10000
#define KILLPERC 13
#define LEN(i) ((259309*(i))%MAXLEN)

static int test_kibitz(struct obuck_header *h, oid_t new, byte *buck)
{
  return 1;
}

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
  obuck_shakedown(test_kibitz);
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
