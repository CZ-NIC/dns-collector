/*
 *	Sherlock Library -- Fast Buffered I/O
 *
 *	(c) 1997--2000 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_FASTBUF_H
#define _SHERLOCK_FASTBUF_H

#ifndef EOF
#include <stdio.h>
#endif

/*
 *  Generic buffered I/O on a top of buffer swapping functions.
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
 *  +----------------+---------------------------+
 *  | written data   | free space                |
 *  +----------------+---------------------------+
 *  ^                 ^                           ^
 *  buffer=bstop      bptr                        bufend
 */

struct fastbuf {
  byte *bptr, *bstop;			/* Access pointers */
  byte *buffer, *bufend;		/* Start and end of the buffer */
  byte *name;				/* File name for error messages */
  uns buflen;				/* Size of the buffer */
  sh_off_t pos;				/* Position of buffer start in the file */
  sh_off_t fdpos;			/* Current position in the non-buffered file */
  int fd;				/* File descriptor, -1 if not a real file */
  int is_temp_file;			/* Is a temporary file, delete on close */
  void *lldata;				/* Data private to access functions below */
  void *llpos;				/* ... continued ... */
  int (*refill)(struct fastbuf *);	/* Get a buffer with new data */
  void (*spout)(struct fastbuf *);	/* Write buffer data to the file */
  void (*seek)(struct fastbuf *, sh_off_t, int);  /* Slow path for bseek(), buffer already flushed */
  void (*close)(struct fastbuf *);	/* Close the stream */
};

/* FastIO on standard files */

struct fastbuf *bopen(byte *name, uns mode, uns buffer);
struct fastbuf *bfdopen(int fd, uns buffer);
void bbcopy(struct fastbuf *f, struct fastbuf *t, uns l);

/* FastIO on in-memory streams */

struct fastbuf *fbmem_create(unsigned blocksize);	/* Create stream and return its writing fastbuf */
struct fastbuf *fbmem_clone_read(struct fastbuf *);	/* Create reading fastbuf */

/* Universal functions working on all fastbuf's */

void bclose(struct fastbuf *f);
void bflush(struct fastbuf *f);
void bseek(struct fastbuf *f, sh_off_t pos, int whence);
void bsetpos(struct fastbuf *f, sh_off_t pos);

static inline sh_off_t btell(struct fastbuf *f)
{
  return f->pos + (f->bptr - f->buffer);
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

static inline void bungetc(struct fastbuf *f, byte c)
{
  *--f->bptr = c;
}

void bputc_slow(struct fastbuf *f, byte c);
static inline void bputc(struct fastbuf *f, byte c)
{
  if (f->bptr < f->bufend)
    *f->bptr++ = c;
  else
    bputc_slow(f, c);
}

word bgetw_slow(struct fastbuf *f);
static inline word bgetw(struct fastbuf *f)
{
  word w;
  if (f->bptr + 2 <= f->bstop)
    {
      byte *p = f->bptr;
#ifdef CPU_CAN_DO_UNALIGNED_WORDS
      w = * ((word *) p);
#else
#ifdef CPU_BIG_ENDIAN
      w = (p[0] << 8) | p[1];
#else
      w = (p[1] << 8) | p[0];
#endif
#endif
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
      byte *p = f->bptr;
#ifdef CPU_CAN_DO_UNALIGNED_LONGS
      l = * ((u32 *) p);
#else
#ifdef CPU_BIG_ENDIAN
      l = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
#else
      l = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
#endif
#endif
      f->bptr += 4;
      return l;
    }
  else
    return bgetl_slow(f);
}

u64 bgetq_slow(struct fastbuf *f);
static inline u64 bgetq(struct fastbuf *f)
{
  if (f->bptr + 8 <= f->bstop)
    {
      u64 l;
      memcpy(&l, f->bptr, 8);
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
      byte *p = f->bptr;
#ifdef CPU_BIG_ENDIAN
      l = ((u64)p[0] << 32) | (u32)((p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4]);
#else
      l = ((u64)p[4] << 32) | (u32)((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
#endif
      f->bptr += 5;
      return l;
    }
  else
    return bget5_slow(f);
}

void bputw_slow(struct fastbuf *f, word w);
static inline void bputw(struct fastbuf *f, word w)
{
  if (f->bptr + 2 <= f->bufend)
    {
      byte *p = f->bptr;
#ifdef CPU_CAN_DO_UNALIGNED_WORDS
      * ((word *) p) = w;
#else
#ifdef CPU_BIG_ENDIAN
      p[0] = w >> 8U;
      p[1] = w;
#else
      p[1] = w >> 8U;
      p[0] = w;
#endif
#endif
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
      byte *p = f->bptr;
#ifdef CPU_CAN_DO_UNALIGNED_LONGS
      * ((u32 *) p) = l;
#else
#ifdef CPU_BIG_ENDIAN
      p[0] = l >> 24U;
      p[1] = l >> 16U;
      p[2] = l >> 8U;
      p[3] = l;
#else
      p[3] = l >> 24U;
      p[2] = l >> 16U;
      p[1] = l >> 8U;
      p[0] = l;
#endif
#endif
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
      memcpy(f->bptr, &l, 8);
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
      byte *p = f->bptr;
      u32 low = l;
#ifdef CPU_BIG_ENDIAN
      p[0] = l >> 32U;
      p[1] = low >> 24U;
      p[2] = low >> 16U;
      p[3] = low >> 8U;
      p[4] = low;
#else
      p[4] = l >> 32U;
      p[3] = low >> 24U;
      p[2] = low >> 16U;
      p[1] = low >> 8U;
      p[0] = low;
#endif
      f->bptr += 5;
    }
  else
    bput5_slow(f, l);
}

uns bread_slow(struct fastbuf *f, void *b, uns l);
static inline uns bread(struct fastbuf *f, void *b, uns l)
{
  if (f->bptr + l <= f->bstop)
    {
      memcpy(b, f->bptr, l);
      f->bptr += l;
      return l;
    }
  else
    return bread_slow(f, b, l);
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

/* Direct I/O on buffers */

int bdirect_read(struct fastbuf *f, byte **buf);
int bdirect_write_prepare(struct fastbuf *f, byte **buf);
void bdirect_write_commit(struct fastbuf *f, byte *pos);

/* Depending on compile-time configuration, we select the right function for reading/writing of file offsets */

#ifdef SHERLOCK_CONFIG_LARGE_DB
#define bgeto(f) bget5(f)
#define bputo(f,l) bput5(f,l)
#define bgetp(f) bgetq(f)
#define bputp(f,l) bputq(f,l)
#else
#define bgeto(f) bgetl(f)
#define bputo(f,l) bputl(f,l)
#define bgetp(f) bgetl(f)
#define bputp(f,l) bputl(f,l)
#endif

#endif
