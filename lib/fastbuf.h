/*
 *	Sherlock Library -- Fast File Buffering
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef EOF
#include <stdio.h>
#endif

struct fastbuf {
  byte *bptr, *bstop;			/* Access pointers */
  byte *buffer, *bufend;		/* Start and end of the buffer */
  byte *name;				/* File name for error messages */
  uns buflen;				/* Size of standard portion of the buffer */
  uns pos;				/* Position of bptr in the file */
  uns fdpos;				/* Current position in the file */
  int fd;				/* File descriptor */
};

struct fastbuf *bopen(byte *name, uns mode, uns buffer);
struct fastbuf *bfdopen(int fd, uns buffer);
void bclose(struct fastbuf *f);
void bflush(struct fastbuf *f);

void bseek(struct fastbuf *f, uns pos, int whence);
void bsetpos(struct fastbuf *f, uns pos);

extern inline uns btell(struct fastbuf *f)
{
  return f->pos + (f->bptr - f->buffer);
}

int bgetc_slow(struct fastbuf *f);
extern inline int bgetc(struct fastbuf *f)
{
  return (f->bptr < f->bstop) ? (int) *f->bptr++ : bgetc_slow(f);
}

int bpeekc_slow(struct fastbuf *f);
extern inline int bpeekc(struct fastbuf *f)
{
  return (f->bptr < f->bstop) ? (int) *f->bptr : bpeekc_slow(f);
}

extern inline void bungetc(struct fastbuf *f, byte c)
{
  *--f->bptr = c;
}

void bputc_slow(struct fastbuf *f, byte c);
extern inline void bputc(struct fastbuf *f, byte c)
{
  if (f->bptr < f->bufend)
    *f->bptr++ = c;
  else
    bputc_slow(f, c);
}

word bgetw_slow(struct fastbuf *f);
extern inline word bgetw(struct fastbuf *f)
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

ulg bgetl_slow(struct fastbuf *f);
extern inline ulg bgetl(struct fastbuf *f)
{
  ulg l;
  if (f->bptr + 4 <= f->bstop)
    {
      byte *p = f->bptr;
#ifdef CPU_CAN_DO_UNALIGNED_LONGS
      l = * ((ulg *) p);
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

void bputw_slow(struct fastbuf *f, word w);
extern inline void bputw(struct fastbuf *f, word w)
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

void bputl_slow(struct fastbuf *f, ulg l);
extern inline void bputl(struct fastbuf *f, ulg l)
{
  if (f->bptr + 4 <= f->bufend)
    {
      byte *p = f->bptr;
#ifdef CPU_CAN_DO_UNALIGNED_LONGS
      * ((ulg *) p) = l;
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

void bread_slow(struct fastbuf *f, void *b, uns l);
extern inline void bread(struct fastbuf *f, void *b, uns l)
{
  if (f->bptr + l <= f->bstop)
    {
      memcpy(b, f->bptr, l);
      f->bptr += l;
    }
  else
    bread_slow(f, b, l);
}

void bwrite_slow(struct fastbuf *f, void *b, uns l);
extern inline void bwrite(struct fastbuf *f, void *b, uns l)
{
  if (f->bptr + l <= f->bufend)
    {
      memcpy(f->bptr, b, l);
      f->bptr += l;
    }
  else
    bwrite_slow(f, b, l);
}

void bbcopy(struct fastbuf *f, struct fastbuf *t, uns l);
byte *bgets(struct fastbuf *f, byte *b, uns l);	/* Non-std */

extern inline void
bputs(struct fastbuf *f, byte *b)
{
  bwrite(f, b, strlen(b));
}

extern inline void
bputsn(struct fastbuf *f, byte *b)
{
  bputs(f, b);
  bputc(f, '\n');
}

