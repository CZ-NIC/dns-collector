/*
 *	Sherlock Library -- Fast Buffered I/O
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_FASTBUF_H
#define _SHERLOCK_FASTBUF_H

#ifndef EOF
#include <stdio.h>
#endif

#include <string.h>

#include "lib/unaligned.h"

/*
 *  Generic buffered I/O. You supply hooks to be called for low-level operations
 *  (swapping of buffers, seeking and closing), we do the rest.
 *
 *  Buffer layout when reading:
 *
 *  +----------------+---------------------------+
 *  | read data      | free space                |
 *  +----------------+---------------------------+
 *  ^        ^        ^                           ^
 *  buffer   bptr     bstop                       bufend
 *
 *  After the last character is read, bptr == bstop and buffer refill
 *  is deferred to the next read attempt. This gives us an easy way
 *  how to implement bungetc().
 *
 *  When writing:
 *
 *  +--------+--------------+--------------------+
 *  | unused | written data | free space         |
 *  +--------+--------------+--------------------+
 *  ^         ^              ^                    ^
 *  buffer    bstop          bptr                 bufend
 *
 *  Dirty tricks:
 *
 *    - You can mix reads and writes on the same stream, but you must
 *	call bflush() in between and remember that the file position
 *	points after the flushed buffer which is not necessarily the same
 *	as after the data you've read.
 *    - The spout/refill hooks can change not only bptr and bstop, but also
 *	the location of the buffer; fb-mem.c takes advantage of it.
 *    - In some cases, the user of the bdirect interface can be allowed to modify
 *	the data in the buffer to avoid unnecessary copying. If the back-end
 *	allows such modifications, it can set can_overwrite_buffer accordingly:
 *		*  0 if no modification is allowed,
 *		*  1 if the user can modify the buffer on the condition that
 *		     the modifications will be undone before calling the next
 *		     fastbuf operation
 *		*  2 if the user is allowed to overwrite the data in the buffer
 *		     if bdirect_read_commit_modified() is called afterwards.
 *		     In this case, the back-end must be prepared for trimming
 *		     of the buffer which is done by the commit function.
 */

struct fastbuf {
  byte is_fastbuf[0];			/* Dummy field for checking of type casts */
  byte *bptr, *bstop;			/* Access pointers */
  byte *buffer, *bufend;		/* Start and end of the buffer */
  byte *name;				/* File name for error messages */
  sh_off_t pos;				/* Position of bstop in the file */
  int (*refill)(struct fastbuf *);	/* Get a buffer with new data */
  void (*spout)(struct fastbuf *);	/* Write buffer data to the file */
  void (*seek)(struct fastbuf *, sh_off_t, int);  /* Slow path for bseek(), buffer already flushed */
  void (*close)(struct fastbuf *);	/* Close the stream */
  int (*config)(struct fastbuf *, uns, int);	/* Configure the stream */
  int can_overwrite_buffer;		/* Can the buffer be altered? (see discussion above) 0=never, 1=temporarily, 2=permanently */
};

/* FastIO on standard files (specify buffer size 0 to enable mmaping) */

struct fastbuf *bopen(byte *name, uns mode, uns buflen);
struct fastbuf *bopen_tmp(uns buflen);
struct fastbuf *bfdopen(int fd, uns buflen);
struct fastbuf *bfdopen_shared(int fd, uns buflen);
void bfilesync(struct fastbuf *b);

/* FastIO on in-memory streams */

struct fastbuf *fbmem_create(unsigned blocksize);	/* Create stream and return its writing fastbuf */
struct fastbuf *fbmem_clone_read(struct fastbuf *);	/* Create reading fastbuf */

/* FastIO on memory mapped files */

struct fastbuf *bopen_mm(byte *name, uns mode);

/* FastI on file descriptors with limit */

struct fastbuf *bopen_limited_fd(int fd, uns bufsize, uns limit);

/* FastIO on static buffers */

void fbbuf_init_read(struct fastbuf *f, byte *buffer, uns size, uns can_overwrite);
void fbbuf_init_write(struct fastbuf *f, byte *buffer, uns size);
static inline uns
fbbuf_count_written(struct fastbuf *f)
{
  return f->bptr - f->bstop;
}

/* Configuring stream parameters */

int bconfig(struct fastbuf *f, uns type, int data);

#define BCONFIG_IS_TEMP_FILE 0

/* Universal functions working on all fastbuf's */

void bclose(struct fastbuf *f);
void bflush(struct fastbuf *f);
void bseek(struct fastbuf *f, sh_off_t pos, int whence);
void bsetpos(struct fastbuf *f, sh_off_t pos);
void brewind(struct fastbuf *f);
int bskip(struct fastbuf *f, uns len);
sh_off_t bfilesize(struct fastbuf *f);

static inline sh_off_t btell(struct fastbuf *f)
{
  return f->pos + (f->bptr - f->bstop);
}

int bgetc_slow(struct fastbuf *f);
static inline int bgetc(struct fastbuf *f)
{
  return (f->bptr < f->bstop) ? (int) *f->bptr++ : bgetc_slow(f);
}

int bpeekc_slow(struct fastbuf *f);
static inline int bpeekc(struct fastbuf *f)
{
  return (f->bptr < f->bstop) ? (int) *f->bptr : bpeekc_slow(f);
}

static inline void bungetc(struct fastbuf *f)
{
  f->bptr--;
}

void bputc_slow(struct fastbuf *f, uns c);
static inline void bputc(struct fastbuf *f, uns c)
{
  if (f->bptr < f->bufend)
    *f->bptr++ = c;
  else
    bputc_slow(f, c);
}

static inline uns
bavailr(struct fastbuf *f)
{
  return f->bstop - f->bptr;
}

static inline uns
bavailw(struct fastbuf *f)
{
  return f->bufend - f->bptr;
}

int bgetw_slow(struct fastbuf *f);
static inline int bgetw(struct fastbuf *f)
{
  int w;
  if (bavailr(f) >= 2)
    {
      w = GET_U16(f->bptr);
      f->bptr += 2;
      return w;
    }
  else
    return bgetw_slow(f);
}

u32 bgetl_slow(struct fastbuf *f);
static inline u32 bgetl(struct fastbuf *f)
{
  u32 l;
  if (bavailr(f) >= 4)
    {
      l = GET_U32(f->bptr);
      f->bptr += 4;
      return l;
    }
  else
    return bgetl_slow(f);
}

u64 bgetq_slow(struct fastbuf *f);
static inline u64 bgetq(struct fastbuf *f)
{
  u64 l;
  if (bavailr(f) >= 8)
    {
      l = GET_U64(f->bptr);
      f->bptr += 8;
      return l;
    }
  else
    return bgetq_slow(f);
}

u64 bget5_slow(struct fastbuf *f);
static inline u64 bget5(struct fastbuf *f)
{
  u64 l;
  if (bavailr(f) >= 5)
    {
      l = GET_U40(f->bptr);
      f->bptr += 5;
      return l;
    }
  else
    return bget5_slow(f);
}

void bputw_slow(struct fastbuf *f, uns w);
static inline void bputw(struct fastbuf *f, uns w)
{
  if (bavailw(f) >= 2)
    {
      PUT_U16(f->bptr, w);
      f->bptr += 2;
    }
  else
    bputw_slow(f, w);
}

void bputl_slow(struct fastbuf *f, u32 l);
static inline void bputl(struct fastbuf *f, u32 l)
{
  if (bavailw(f) >= 4)
    {
      PUT_U32(f->bptr, l);
      f->bptr += 4;
    }
  else
    bputl_slow(f, l);
}

void bputq_slow(struct fastbuf *f, u64 l);
static inline void bputq(struct fastbuf *f, u64 l)
{
  if (bavailw(f) >= 8)
    {
      PUT_U64(f->bptr, l);
      f->bptr += 8;
    }
  else
    bputq_slow(f, l);
}

void bput5_slow(struct fastbuf *f, u64 l);
static inline void bput5(struct fastbuf *f, u64 l)
{
  if (bavailw(f) >= 5)
    {
      PUT_U40(f->bptr, l);
      f->bptr += 5;
    }
  else
    bput5_slow(f, l);
}

uns bread_slow(struct fastbuf *f, void *b, uns l, uns check);
static inline uns bread(struct fastbuf *f, void *b, uns l)
{
  if (bavailr(f) >= l)
    {
      memcpy(b, f->bptr, l);
      f->bptr += l;
      return l;
    }
  else
    return bread_slow(f, b, l, 0);
}

static inline uns breadb(struct fastbuf *f, void *b, uns l)
{
  if (bavailr(f) >= l)
    {
      memcpy(b, f->bptr, l);
      f->bptr += l;
      return l;
    }
  else
    return bread_slow(f, b, l, 1);
}

void bwrite_slow(struct fastbuf *f, void *b, uns l);
static inline void bwrite(struct fastbuf *f, void *b, uns l)
{
  if (bavailw(f) >= l)
    {
      memcpy(f->bptr, b, l);
      f->bptr += l;
    }
  else
    bwrite_slow(f, b, l);
}

byte *bgets(struct fastbuf *f, byte *b, uns l);	/* Non-std */
int bgets_nodie(struct fastbuf *f, byte *b, uns l);
byte *bgets0(struct fastbuf *f, byte *b, uns l);

static inline void
bputs(struct fastbuf *f, byte *b)
{
  bwrite(f, b, strlen(b));
}

static inline void
bputs0(struct fastbuf *f, byte *b)
{
  bwrite(f, b, strlen(b)+1);
}

static inline void
bputsn(struct fastbuf *f, byte *b)
{
  bputs(f, b);
  bputc(f, '\n');
}

void bbcopy_slow(struct fastbuf *f, struct fastbuf *t, uns l);
static inline void
bbcopy(struct fastbuf *f, struct fastbuf *t, uns l)
{
  if (bavailr(f) >= l && bavailw(t) >= l)
    {
      memcpy(t->bptr, f->bptr, l);
      t->bptr += l;
      f->bptr += l;
    }
  else
    bbcopy_slow(f, t, l);
}

/* I/O on addr_int_t */

#ifdef CPU_64BIT_POINTERS
#define bputa(x,p) bputq(x,p)
#define bgeta(x) bgetq(x)
#else
#define bputa(x,p) bputl(x,p)
#define bgeta(x) bgetl(x)
#endif

/* Direct I/O on buffers */

static inline uns
bdirect_read_prepare(struct fastbuf *f, byte **buf)
{
  if (f->bptr == f->bstop && !f->refill(f))
    return 0;
  *buf = f->bptr;
  return bavailr(f);
}

static inline void
bdirect_read_commit(struct fastbuf *f, byte *pos)
{
  f->bptr = pos;
}

static inline void
bdirect_read_commit_modified(struct fastbuf *f, byte *pos)
{
  f->bptr = pos;
  f->buffer = pos;	/* Avoid seeking backwards in the buffer */
}

static inline uns
bdirect_write_prepare(struct fastbuf *f, byte **buf)
{
  if (f->bptr == f->bufend)
    f->spout(f);
  *buf = f->bptr;
  return bavailw(f);
}

static inline void
bdirect_write_commit(struct fastbuf *f, byte *pos)
{
  f->bptr = pos;
}

/* Formatted output */

int bprintf(struct fastbuf *b, byte *msg, ...);
int vbprintf(struct fastbuf *b, byte *msg, va_list args);

#endif
