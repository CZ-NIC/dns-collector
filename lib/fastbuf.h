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
#ifdef CPU_CAN_DO_UNALIGNED_WORDS
  word w;
  if (f->bptr + 2 <= f->bstop)
    {
      w = * ((word *) f->bptr);
      f->bptr += 2;
      return w;
    }
  else
    return bgetw_slow(f);
#else
  word w = bgetc(f);
#ifdef CPU_BIG_ENDIAN
  return (w << 8) | bgetc(f);
#else
  return w | (bgetc(f) << 8);
#endif
#endif
}

ulg bgetl_slow(struct fastbuf *f);
extern inline ulg bgetl(struct fastbuf *f)
{
#ifdef CPU_CAN_DO_UNALIGNED_LONGS
  ulg l;
  if (f->bptr + 4 <= f->bstop)
    {
      l = * ((ulg *) f->bptr);
      f->bptr += 4;
      return l;
    }
  else
    return bgetl_slow(f);
#else
  ulg l = bgetc(f);
#ifdef CPU_BIG_ENDIAN
  l = (l << 8) | bgetc(f);
  l = (l << 8) | bgetc(f);
  return (l << 8) | bgetc(f);
#else
  l = (bgetc(f) << 8) | l;
  l = (bgetc(f) << 16) | l;
  return (bgetc(f) << 24) | l;
#endif
#endif
}

void bputw_slow(struct fastbuf *f, word w);
extern inline void bputw(struct fastbuf *f, word w)
{
#ifdef CPU_CAN_DO_UNALIGNED_WORDS
  if (f->bptr + 2 <= f->bufend)
    {
      * ((word *) f->bptr) = w;
      f->bptr += 2;
    }
  else
    bputw_slow(f, w);
#else
#ifdef CPU_BIG_ENDIAN
  bputc(f, w >> 8);
  bputc(f, w);
#else
  bputc(f, w);
  bputc(f, w >> 8);
#endif
#endif
}

void bputl_slow(struct fastbuf *f, ulg l);
extern inline void bputl(struct fastbuf *f, ulg l)
{
#ifdef CPU_CAN_DO_UNALIGNED_LONGS
  if (f->bptr + 4 <= f->bufend)
    {
      * ((ulg *) f->bptr) = l;
      f->bptr += 4;
    }
  else
    bputl_slow(f, l);
#else
#ifdef CPU_BIG_ENDIAN
  bputc(f, l >> 24);
  bputc(f, l >> 16);
  bputc(f, l >> 8);
  bputc(f, l);
#else
  bputc(f, l);
  bputc(f, l >> 8);
  bputc(f, l >> 16);
  bputc(f, l >> 24);
#endif
#endif
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

extern inline byte *			/* Non-standard */
bgets(struct fastbuf *f, byte *b, uns l)
{
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k == EOF)
    return NULL;
  while (b < e)
    {
      if (k == '\n' || k == EOF)
	{
	  *b = 0;
	  return b;
	}
      *b++ = k;
      k = bgetc(f);
    }
  die("%s: Line too long", f->name);
}

extern inline void
bputs(struct fastbuf *f, byte *b)
{
  while (*b)
    bputc(f, *b++);
}

extern inline void
bputsn(struct fastbuf *f, byte *b)
{
  bputs(f, b);
  bputc(f, '\n');
}

