/*
 *	Sherlock Library -- Fast Buffered I/O
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
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
};

/* FastIO on standard files (specify buffer size 0 to enable mmaping) */

struct fastbuf *bopen(byte *name, uns mode, uns buffer);
struct fastbuf *bopen_tmp(uns buffer);
struct fastbuf *bfdopen(int fd, uns buffer);
struct fastbuf *bfdopen_shared(int fd, uns buffer);

/* FastIO on in-memory streams */

struct fastbuf *fbmem_create(unsigned blocksize);	/* Create stream and return its writing fastbuf */
struct fastbuf *fbmem_clone_read(struct fastbuf *);	/* Create reading fastbuf */

/* FastIO on memory mapped files */

struct fastbuf *bopen_mm(byte *name, uns mode);

/* FastI on file descriptors with limit */

struct fastbuf *bopen_limited_fd(int fd, uns bufsize, uns limit);

/* FastIO on static buffers */

void fbbuf_init_read(struct fastbuf *f, byte *buffer, uns size);
void fbbuf_init_write(struct fastbuf *f, byte *buffer, uns size);
static inline uns
fbbuf_count_written(struct fastbuf *f)
{
  return f->bptr - f->bstop;
}

/* Configuring stream parameters */

int bconfig(struct fastbuf *f, uns type, int data);

#define BCONFIG_IS_TEMP_FILE 0
#define BCONFIG_CAN_OVERWRITE 1

/* Universal functions working on all fastbuf's */

void bclose(struct fastbuf *f);
void bflush(struct fastbuf *f);
void bseek(struct fastbuf *f, sh_off_t pos, int whence);
void bsetpos(struct fastbuf *f, sh_off_t pos);
void brewind(struct fastbuf *f);

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

int bgetw_slow(struct fastbuf *f);
static inline int bgetw(struct fastbuf *f)
{
  int w;
  if (f->bptr + 2 <= f->bstop)
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
  if (f->bptr + 4 <= f->bstop)
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
  if (f->bptr + 8 <= f->bstop)
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
  if (f->bptr + 5 <= f->bstop)
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
  if (f->bptr + 2 <= f->bufend)
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
  if (f->bptr + 4 <= f->bufend)
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
  if (f->bptr + 8 <= f->bufend)
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
  if (f->bptr + 5 <= f->bufend)
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
  if (f->bptr + l <= f->bstop)
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
  if (f->bptr + l <= f->bstop)
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
  if (f->bptr + l <= f->bufend)
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
  if (f->bptr + l <= f->bstop &&
      t->bptr + l <= t->bufend)
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
  return f->bstop - f->bptr;
}

static inline void
bdirect_read_commit(struct fastbuf *f, byte *pos)
{
  f->bptr = pos;
}

static inline uns
bdirect_write_prepare(struct fastbuf *f, byte **buf)
{
  if (f->bptr == f->bufend)
    f->spout(f);
  *buf = f->bptr;
  return f->bufend - f->bptr;
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
