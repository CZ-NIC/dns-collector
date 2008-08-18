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

/***
 * Generic buffered I/O. You supply hooks to be called for low-level operations
 * (swapping of buffers, seeking and closing), we do the rest.
 *
 * Buffer layout when reading:
 *
 *  +----------------+---------------------------+
 *  | read data      | free space                |
 *  +----------------+---------------------------+
 *  ^        ^        ^                           ^
 *  buffer   bptr     bstop                       bufend
 *
 * After the last character is read, +bptr == bstop+ and buffer refill
 * is deferred to the next read attempt. This gives us an easy way
 * how to implement bungetc().
 *
 * When writing:
 *
 *  +--------+--------------+--------------------+
 *  | unused | written data | free space         |
 *  +--------+--------------+--------------------+
 *  ^         ^              ^                    ^
 *  buffer    bstop          bptr                 bufend
 *
 * Dirty tricks:
 *
 *    - You can mix reads and writes on the same stream, but you must
 *	call bflush() in between and remember that the file position
 *	points after the flushed buffer which is not necessarily the same
 *	as after the data you've read.
 *    - The spout/refill hooks can change not only bptr and bstop, but also
 *	the location of the buffer; +fb-mem.c+ takes advantage of it.
 *    - In some cases, the user of the +bdirect+ interface can be allowed to modify
 *	the data in the buffer to avoid unnecessary copying. If the back-end
 *	allows such modifications, it can set +can_overwrite_buffer+ accordingly:
 *		*  0 if no modification is allowed,
 *		*  1 if the user can modify the buffer on the condition that
 *		     the modifications will be undone before calling the next
 *		     fastbuf operation
 *		*  2 if the user is allowed to overwrite the data in the buffer
 *		     if bdirect_read_commit_modified() is called afterwards.
 *		     In this case, the back-end must be prepared for trimming
 *		     of the buffer which is done by the commit function.
 *
 * Generic parts
 * ~~~~~~~~~~~~~
 ***/

/**
 * Fastbuf structure.
 * This structure is of main interest to fastbuf backends,
 * it can be considered a black box for use.
 **/
struct fastbuf {
  byte is_fastbuf[0];				/* Dummy field for checking of type casts */
  byte *bptr, *bstop;				/* Access pointers */
  byte *buffer, *bufend;			/* Start and end of the buffer */
  char *name;					/* File name for error messages */
  ucw_off_t pos;				/* Position of bstop in the file */
  int (*refill)(struct fastbuf *);		/* Get a buffer with new data */
  void (*spout)(struct fastbuf *);		/* Write buffer data to the file */
  int (*seek)(struct fastbuf *, ucw_off_t, int);/* Slow path for bseek(), buffer already flushed; returns success */
  void (*close)(struct fastbuf *);		/* Close the stream */
  int (*config)(struct fastbuf *, uns, int);	/* Configure the stream */
  int can_overwrite_buffer;			/* Can the buffer be altered? (see discussion above) 0=never, 1=temporarily, 2=permanently */
};

/*** === FastIO on files with several configurable back-ends ***/

/**
 * Which back-end do you want to use?
 */
enum fb_type {
  FB_STD,				/* Standard buffered I/O */
  FB_DIRECT,				/* Direct I/O bypassing system caches (see +fb-direct.c+ for a description) */
  FB_MMAP				/* Memory mapped files */
};

/**
 * A way to configure created fastbuf.
 */
struct fb_params {
  enum fb_type type;
  uns buffer_size;			/* 0 for default size. */
  uns keep_back_buf;			/* FB_STD: optimize for bi-directional access. */
  uns read_ahead;			/* FB_DIRECT options. */
  uns write_back;
  struct asio_queue *asio;
};

struct cf_section;
extern struct cf_section fbpar_cf; /** Config. Can be used by fastbuf systems. **/
extern struct fb_params fbpar_def; /** Default parameters. **/

/**
 * Opens a file.
 * Use +@params = NULL+ for defaults.
 * See standard unix open() for information about @mode.
 **/
struct fastbuf *bopen_file(const char *name, int mode, struct fb_params *params);
struct fastbuf *bopen_file_try(const char *name, int mode, struct fb_params *params); /** Tries to open a file (does not die, if unsuccessful). **/

/**
 * Opens a temporary file.
 * It is placed with other temp files and is deleted when closed.
 **/
struct fastbuf *bopen_tmp_file(struct fb_params *params);
/**
 * Creates a fastbuf (wrapper) from a file descriptor.
 * Sets it's filename to @name (used when outputting errors).
 */
struct fastbuf *bopen_fd_name(int fd, struct fb_params *params, const char *name);
static inline struct fastbuf *bopen_fd(int fd, struct fb_params *params) /** Same as above, but with empty filename. **/
{
  return bopen_fd_name(fd, params, NULL);
}

/***
 * FastIO on standard files (shortcuts for FB_STD)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ***/

/**
 * Opens a file in a usual way (with system cache enabled).
 * Use +@buflen = 0+ for default buffer size.
 * Dies if unsuccessful.
 */
struct fastbuf *bopen(const char *name, uns mode, uns buflen);
struct fastbuf *bopen_try(const char *name, uns mode, uns buflen);/** Same as bopen(), but does not die when unsuccessful. **/
struct fastbuf *bopen_tmp(uns buflen);/** Opens a temporary file (read-write). Deletes it, when closed. **/
struct fastbuf *bfdopen(int fd, uns buflen);/** Wraps a filedescriptor into a fastbuf. **/
struct fastbuf *bfdopen_shared(int fd, uns buflen);/** Wraps a filedescriptor and marks it as shared. **/
void bfilesync(struct fastbuf *b);/** Sync file to disk. **/

/*** === Temporary files ***/

#define TEMP_FILE_NAME_LEN 256 /** Maximum length of temp file name. **/
/**
 * Generates a temporary filename.
 * Provide a buffer (as @name_buf, at last +TEMP_FILE_NAME_LEN+ long) to store the name into.
 * If @open_flags are not +NULL+, flags that should be ored with other flags to open() will be set.
 *
 * The provided name can already exist.
 * If it is not safe to overwrite existing files, +O_EXCL+ is specified in @open_flags.
 * Check for the result of open().
 *
 * This is not specific to fastbufs, can be used separately.
 **/
void temp_file_name(char *name_buf, int *open_flags);
/**
 * Renames a temp fastbuf to given @name and marks it permanent (so it will not be deleted when closed).
 * The fastbuf is closed by this call.
 */
void bfix_tmp_file(struct fastbuf *fb, const char *name);
/**
 * Opens a temporary file and returns it as file descriptor.
 * You specify open @mode and @open_flags.
 *
 * If @name_buf (at last +TEMP_FILE_NAME_LEN+ long) is not +NULL+, the filename is stored there.
 *
 * This is not specific to fastbufs, can be used separately.
 */
int open_tmp(char *name_buf, int open_flags, int mode);

/* Internal functions of some file back-ends */

struct fastbuf *bfdopen_internal(int fd, const char *name, uns buflen);
struct fastbuf *bfmmopen_internal(int fd, const char *name, uns mode);

extern uns fbdir_cheat;
struct asio_queue;
struct fastbuf *fbdir_open_fd_internal(int fd, const char *name, struct asio_queue *io_queue, uns buffer_size, uns read_ahead, uns write_back);

void bclose_file_helper(struct fastbuf *f, int fd, int is_temp_file);

/***
 * FastIO on in-memory streams
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * These behaves in a way similar to unix pipes.
 * You create one for writing and another for reading.
 * What you write to the first one can be read from the other.
 ***/

struct fastbuf *fbmem_create(uns blocksize);		/** Create stream and return its writing fastbuf. **/
struct fastbuf *fbmem_clone_read(struct fastbuf *);	/** Create reading fastbuf. **/

/*** === FastI on file descriptors with limit ***/

/**
 * Wrap a file descriptor @fd into a fastbuf.
 * No more than @limit bytes will be read/written in the lifetime of this fastbuf.
 **/
struct fastbuf *bopen_limited_fd(int fd, uns bufsize, uns limit);

/*** === FastIO on static buffers ***/

/**
 * Creates a fastbuf that takes data from a memory buffer.
 * The fastbuf is not allocated, it is initialized in @f.
 * @buffer and @size specify the buffer with data.
 * See top of this file for info about @can_overwrite.
 *
 * No resources are alocated by this, so you do not need to free it.
 * It is not possible to close this fastbuf.
 */
void fbbuf_init_read(struct fastbuf *f, byte *buffer, uns size, uns can_overwrite);
/**
 * Creates a fastbuf which writes into a provided memory buffer.
 * Write over the end dies.
 *
 * No resources are allocated by this and you can not close this fastbuf.
 *
 * Data are written directly into the buffer, no need for flushes.
 */
void fbbuf_init_write(struct fastbuf *f, byte *buffer, uns size);
static inline uns fbbuf_count_written(struct fastbuf *f) /** How many bytes were written into the buffer already? **/
{
  return f->bptr - f->bstop;
}

/*** === FastIO on recyclable growing buffers ***/

struct fastbuf *fbgrow_create(unsigned basic_size);	/** Create the growing buffer, with pre-allocated size @basic_size. **/
void fbgrow_reset(struct fastbuf *b);			/** Reset stream and prepare for writing. **/
void fbgrow_rewind(struct fastbuf *b);			/** Prepare for reading (of already written data). **/

/***
 * FastO on memory pools
 * ~~~~~~~~~~~~~~~~~~~~~
 * You write to it and get buffers of written data.
 ***/

struct mempool;
struct fbpool { /** Structure for fastbufs & mempools. **/
  struct fastbuf fb;
  struct mempool *mp;
};

void fbpool_init(struct fbpool *fb);	/** Initialize a new mempool fastbuf. **/
/**
 * Start a new continuous block and prepare for writing (see mp_start()).
 * Provide the memory pool you want to use for this block (in @mp).
 **/
void fbpool_start(struct fbpool *fb, struct mempool *mp, uns init_size);
/**
 * Close the block and return its address (see mp_end()).
 * The length can be determined with mp_size(mp, ptr).
 **/
void *fbpool_end(struct fbpool *fb);

/***
 * === FastO with atomic writes for multi-threaded programs
 * Use them, when you need to write records into single file from several threads.
 * It does not ensure order of the records, but they will not intersect.
 ***/

struct fb_atomic {
  struct fastbuf fb;
  struct fb_atomic_file *af;
  byte *expected_max_bptr;
  uns slack_size;
};
#define FB_ATOMIC(f) ((struct fb_atomic *)(f)->is_fastbuf)

/**
 * Open an atomic fastbuf.
 * If you specify @master, it is used to write into it (both the master
 * and the new one will be the same file, with separate buffers).
 * If @master is +NULL+, a file @name is opened.
 *
 * You can specify @record_len, if it is known (for optimisations).
 *
 * The file is closed when all fastbufs using it are closed.
 **/
struct fastbuf *fbatomic_open(const char *name, struct fastbuf *master, uns bufsize, int record_len);
void fbatomic_internal_write(struct fastbuf *b);

/**
 * Commit the last record.
 * It may not yet write it to the file, but it will stay together.
 **/
static inline void fbatomic_commit(struct fastbuf *b)
{
  if (b->bptr >= ((struct fb_atomic *)b)->expected_max_bptr)
    fbatomic_internal_write(b);
}

/*** === Configuring stream parameters ***/

enum bconfig_type {			/** Parameters that could be configured. **/
  BCONFIG_IS_TEMP_FILE,			/* 0=normal file, 1=temporary file, 2=shared fd */
  BCONFIG_KEEP_BACK_BUF,		/* Optimize for bi-directional access */
};

int bconfig(struct fastbuf *f, uns type, int data); /** Configure a fastbuf. Returns previous value. **/

/*** === Universal functions working on all fastbuf's ***/

/**
 * Close and free fastbuf.
 * Can not be used for fastbufs not returned from function (initialized in a parameter, for example the one from +fbbuf_init_read+).
 */
void bclose(struct fastbuf *f);
void bflush(struct fastbuf *f);					/** Write data (if it makes any sense, do not use for in-memory buffers). **/
void bseek(struct fastbuf *f, ucw_off_t pos, int whence);	/** Seek in the buffer. See +man fseek+ for description of @whence. Only for seekable fastbufs. **/
void bsetpos(struct fastbuf *f, ucw_off_t pos);			/** Set position to @pos bytes from beginning. Only for seekable fastbufs. **/
void brewind(struct fastbuf *f);				/** Go to the beginning of the fastbuf. Only for seekable ones. **/
ucw_off_t bfilesize(struct fastbuf *f);				/** How large is the file? -1 if not seekable. **/

static inline ucw_off_t btell(struct fastbuf *f)		/** Where am I (from the beginning)? **/
{
  return f->pos + (f->bptr - f->bstop);
}

int bgetc_slow(struct fastbuf *f);
static inline int bgetc(struct fastbuf *f)			/** Return next character from the buffer. **/
{
  return (f->bptr < f->bstop) ? (int) *f->bptr++ : bgetc_slow(f);
}

int bpeekc_slow(struct fastbuf *f);
static inline int bpeekc(struct fastbuf *f)			/** Return next character from the buffer, but keep the current position. **/
{
  return (f->bptr < f->bstop) ? (int) *f->bptr : bpeekc_slow(f);
}

static inline void bungetc(struct fastbuf *f)			/** Return last read character back. Only one back is guaranteed to work. **/
{
  f->bptr--;
}

void bputc_slow(struct fastbuf *f, uns c);
static inline void bputc(struct fastbuf *f, uns c)		/** Write a single character. **/
{
  if (f->bptr < f->bufend)
    *f->bptr++ = c;
  else
    bputc_slow(f, c);
}

static inline uns bavailr(struct fastbuf *f)
{
  return f->bstop - f->bptr;
}

static inline uns bavailw(struct fastbuf *f)
{
  return f->bufend - f->bptr;
}

uns bread_slow(struct fastbuf *f, void *b, uns l, uns check);
/**
 * Read at most @l bytes of data into @b.
 * Returns number of bytes read.
 * 0 means end of file.
 */
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

/**
 * Reads exactly @l bytes of data into @b.
 * If at the end of file, it returns 0.
 * If there are data, but less than @l, it dies.
 */
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

void bwrite_slow(struct fastbuf *f, const void *b, uns l);
static inline void bwrite(struct fastbuf *f, const void *b, uns l) /** Writes buffer @b of length @l into fastbuf. **/
{
  if (bavailw(f) >= l)
    {
      memcpy(f->bptr, b, l);
      f->bptr += l;
    }
  else
    bwrite_slow(f, b, l);
}

/**
 * Reads a line into @b and strips trailing +\n+.
 * Returns pointer to the terminating 0 or +NULL+ on EOF.
 * Dies if the line is longer than @l.
 **/
char *bgets(struct fastbuf *f, char *b, uns l);
char *bgets0(struct fastbuf *f, char *b, uns l);	/** The same as bgets(), but for 0-terminated strings. **/
/**
 * Returns either length of read string (excluding the terminator) or -1 if it is too long.
 * In such cases exactly @l bytes are read.
 */
int bgets_nodie(struct fastbuf *f, char *b, uns l);

struct mempool;
struct bb_t;
/**
 * Read a string, strip the trailing +\n+ and store it into growing buffer @b.
 * Dies if the line is longer than @limit.
 **/
uns bgets_bb(struct fastbuf *f, struct bb_t *b, uns limit);
/**
 * Read a string, strip the trailing +\n+ and store it into buffer allocated from a memory pool.
 **/
char *bgets_mp(struct fastbuf *f, struct mempool *mp);

struct bgets_stk_struct {
  struct fastbuf *f;
  byte *old_buf, *cur_buf, *src;
  uns old_len, cur_len, src_len;
};
void bgets_stk_init(struct bgets_stk_struct *s);
void bgets_stk_step(struct bgets_stk_struct *s);

/**
 * Read a string, strip the trailing +\n+ and store it on the stack (allocated using alloca()).
 **/
#define bgets_stk(fb) \
  ({ struct bgets_stk_struct _s; _s.f = (fb); for (bgets_stk_init(&_s); _s.cur_len; _s.cur_buf = alloca(_s.cur_len), bgets_stk_step(&_s)); _s.cur_buf; })

/**
 * Write a string, without 0 or +\n+ at the end.
 **/
static inline void bputs(struct fastbuf *f, const char *b)
{
  bwrite(f, b, strlen(b));
}

/**
 * Write string, including terminating 0.
 **/
static inline void bputs0(struct fastbuf *f, const char *b)
{
  bwrite(f, b, strlen(b)+1);
}

/**
 * Write string and append a newline to the end.
 **/
static inline void bputsn(struct fastbuf *f, const char *b)
{
  bputs(f, b);
  bputc(f, '\n');
}

void bbcopy_slow(struct fastbuf *f, struct fastbuf *t, uns l);
/**
 * Copy @l bytes of data from fastbuf @f to fastbuf @t.
 **/
static inline void bbcopy(struct fastbuf *f, struct fastbuf *t, uns l)
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
static inline int bskip(struct fastbuf *f, uns len) /** Skip @len bytes without reading them. **/
{
  if (bavailr(f) >= len)
    {
      f->bptr += len;
      return 1;
    }
  else
    return bskip_slow(f, len);
}

/*** === Direct I/O on buffers ***/
// TODO Documentation -- what do they do?

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

/*** === Formatted output ***/

/**
 * printf into a fastbuf.
 **/
int bprintf(struct fastbuf *b, const char *msg, ...)
  FORMAT_CHECK(printf,2,3);
int vbprintf(struct fastbuf *b, const char *msg, va_list args); /** vprintf into a fastbuf. **/

#endif
