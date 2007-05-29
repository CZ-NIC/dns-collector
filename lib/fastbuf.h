/*
 *	UCW Library -- Fast Buffered I/O
 *
 *	(c) 1997--2007 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_FASTBUF_H
#define _UCW_FASTBUF_H

#include <string.h>
#include <alloca.h>

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
  int (*seek)(struct fastbuf *, sh_off_t, int);  /* Slow path for bseek(), buffer already flushed; returns success */
  void (*close)(struct fastbuf *);	/* Close the stream */
  int (*config)(struct fastbuf *, uns, int);	/* Configure the stream */
  int can_overwrite_buffer;		/* Can the buffer be altered? (see discussion above) 0=never, 1=temporarily, 2=permanently */
};

/* FastIO on standard files (specify buffer size 0 to enable mmaping) */

struct fastbuf *bfdopen_internal(int fd, uns buflen, byte *name);
struct fastbuf *bopen(byte *name, uns mode, uns buflen);
struct fastbuf *bopen_try(byte *name, uns mode, uns buflen);
struct fastbuf *bopen_tmp(uns buflen);
struct fastbuf *bfdopen(int fd, uns buflen);
struct fastbuf *bfdopen_shared(int fd, uns buflen);
void bfilesync(struct fastbuf *b);

#define TEMP_FILE_NAME_LEN 256
void temp_file_name(byte *name);

/* FastIO on in-memory streams */

struct fastbuf *fbmem_create(unsigned blocksize);	/* Create stream and return its writing fastbuf */
struct fastbuf *fbmem_clone_read(struct fastbuf *);	/* Create reading fastbuf */

/* FastIO on memory mapped files */

struct fastbuf *bopen_mm(byte *name, uns mode);

/* FastIO on files opened with O_DIRECT (see fb-direct.c for description) */

extern uns fbdir_cheat;

struct asio_queue;
struct fastbuf *fbdir_open_fd_internal(int fd, struct asio_queue *io_queue, byte *name);
struct fastbuf *fbdir_open(byte *name, uns mode, struct asio_queue *io_queue);
struct fastbuf *fbdir_open_try(byte *name, uns mode, struct asio_queue *io_queue);
struct fastbuf *fbdir_open_fd(int fd, struct asio_queue *io_queue);
struct fastbuf *fbdir_open_tmp(struct asio_queue *io_queue);

/* FastIO on files with run-time parametrization */

enum fb_type {
  FB_STD,
  FB_DIRECT,
  FB_MMAP
};

struct fb_params {
  enum fb_type type;
  uns buffer_size;
  struct asio_queue *asio;
};

struct cf_section;
extern struct cf_section fbpar_cf;
extern struct fb_params fbpar_def;

struct fastbuf *bopen_file(byte *name, int mode, struct fb_params *params);
struct fastbuf *bopen_file_try(byte *name, int mode, struct fb_params *params);
struct fastbuf *bopen_tmp_file(struct fb_params *params);
struct fastbuf *bopen_fd(int fd, struct fb_params *params);

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

/* FastIO on recyclable growing buffers */

struct fastbuf *fbgrow_create(unsigned basic_size);
void fbgrow_reset(struct fastbuf *b);			/* Reset stream and prepare for writing */
void fbgrow_rewind(struct fastbuf *b);			/* Prepare for reading */

/* FastO with atomic writes for multi-threaded programs */

struct fb_atomic {
  struct fastbuf fb;
  struct fb_atomic_file *af;
  byte *expected_max_bptr;
  uns slack_size;
};
#define FB_ATOMIC(f) ((struct fb_atomic *)(f)->is_fastbuf)

struct fastbuf *fbatomic_open(byte *name, struct fastbuf *master, uns bufsize, int record_len);
void fbatomic_internal_write(struct fastbuf *b);

static inline void
fbatomic_commit(struct fastbuf *b)
{
  if (b->bptr >= ((struct fb_atomic *)b)->expected_max_bptr)
    fbatomic_internal_write(b);
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
sh_off_t bfilesize(struct fastbuf *f);		// -1 if not seekable

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

struct mempool;
struct bb_t;
uns bgets_bb(struct fastbuf *f, struct bb_t *b, uns limit);
byte *bgets_mp(struct fastbuf *f, struct mempool *mp);

struct bgets_stk_struct {
  struct fastbuf *f;
  byte *old_buf, *cur_buf, *src;
  uns old_len, cur_len, src_len;
};
void bgets_stk_init(struct bgets_stk_struct *s);
void bgets_stk_step(struct bgets_stk_struct *s);
#define bgets_stk(fb) ({ struct bgets_stk_struct _s; _s.f = (fb); for (bgets_stk_init(&_s); _s.cur_len; _s.cur_buf = alloca(_s.cur_len), bgets_stk_step(&_s)); _s.cur_buf; })

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

int bskip_slow(struct fastbuf *f, uns len);
static inline int bskip(struct fastbuf *f, uns len)
{
  if (bavailr(f) >= len)
    {
      f->bptr += len;
      return 1;
    }
  else
    return bskip_slow(f, len);
}

/* Direct I/O on buffers */

static inline uns
bdirect_read_prepare(struct fastbuf *f, byte **buf)
{
  if (f->bptr == f->bstop && !f->refill(f))
    {
      *buf = NULL;  // This is not needed, but it helps to get rid of spurious warnings
      return 0;
    }
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

int bprintf(struct fastbuf *b, char *msg, ...) FORMAT_CHECK(printf,2,3);
int vbprintf(struct fastbuf *b, char *msg, va_list args);

#endif
