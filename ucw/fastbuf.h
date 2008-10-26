/*
 *	UCW Library -- Fast Buffered I/O
 *
 *	(c) 1997--2008 Martin Mares <mj@ucw.cz>
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
 * === Internal structure [[internal]]
 *
 * Generally speaking, a fastbuf consists of a buffer and a set of callbacks.
 * All front-end functions operate on the buffer and if the buffer becomes
 * empty or fills up, they ask the corresponding callback to solve the
 * situation. Back-ends then differ just in the definition of the callbacks.
 * The state of the fastbuf is represented by `struct fastbuf`, which
 * is a simple structure describing the state of the buffer and pointers
 * to the callback functions.
 *
 * When we are reading from the fastbuf, the buffer is laid out as follows:
 *
 *  +----------------+---------------------------+
 *  | read data      | free space                |
 *  +----------------+---------------------------+
 *  ^        ^        ^                           ^
 *  buffer   bptr     bstop                       bufend
 *
 * Here `bptr` points to the next character to be read. After the last character is
 * read, `bptr == bstop` and the `refill` callback gets called upon the next read
 * attempt to bring further data. This gives us an easy way how to implement @bungetc().
 *
 * When writing, the situation looks like:
 *
 *  +--------+--------------+--------------------+
 *  | unused | written data | free space         |
 *  +--------+--------------+--------------------+
 *  ^         ^              ^                    ^
 *  buffer    bstop          bptr                 bufend
 *
 * In this case, the `bptr` points at the position where the next character
 * will be written to. When we want to write, but `bptr == bufend`, we call
 * the `spout` hook to flush the data and get an empty buffer.
 *
 * Several dirty tricks can be played:
 *
 *    - The `spout`/`refill` hooks can change not only `bptr` and `bstop`, but also
 *	the location and size of the buffer; the fb-mem back-end takes advantage of it.
 *    - In some cases, the user of the `bdirect` interface can be allowed to modify
 *	the data in the buffer to avoid unnecessary copying. If the back-end
 *	allows such modifications, it can set `fastbuf->can_overwrite_buffer` accordingly:
 *		*  0 if no modification is allowed,
 *		*  1 if the user can modify the buffer on the condition that
 *		     the modifications will be undone before calling the next
 *		     fastbuf operation
 *		*  2 if the user is allowed to overwrite the data in the buffer
 *		     if @bdirect_read_commit_modified() is called afterwards.
 *		     In this case, the back-end must be prepared for trimming
 *		     of the buffer which is done by the commit function.
 *
 ***/

/**
 * This structure contains the state of the fastbuf. See the discussion above
 * for how it works.
 **/
struct fastbuf {
  byte is_fastbuf[0];				/* Dummy field for checking of type casts */
  byte *bptr, *bstop;				/* State of the buffer */
  byte *buffer, *bufend;			/* Start and end of the buffer */
  char *name;					/* File name (used for error messages) */
  ucw_off_t pos;				/* Position of bstop in the file */
  int (*refill)(struct fastbuf *);		/* Get a buffer with new data, returns 0 on EOF */
  void (*spout)(struct fastbuf *);		/* Write buffer data to the file */
  int (*seek)(struct fastbuf *, ucw_off_t, int);/* Slow path for @bseek(), buffer already flushed; returns success */
  void (*close)(struct fastbuf *);		/* Close the stream */
  int (*config)(struct fastbuf *, uns, int);	/* Configure the stream */
  int can_overwrite_buffer;			/* Can the buffer be altered? 0=never, 1=temporarily, 2=permanently */
};

/***
 * === Fastbuf on files [[fbparam]]
 *
 * If you want to use fastbufs to access files, you can choose one of several
 * back-ends and set their parameters.
 ***/

/**
 * Back-end types
 */
enum fb_type {
  FB_STD,				/* Standard buffered I/O */
  FB_DIRECT,				/* Direct I/O bypassing system caches (see fb-direct.c for a description) */
  FB_MMAP				/* Memory mapped files */
};

/**
 * When you open a file fastbuf, you can use this structure to select a back-end
 * and set its parameters. If you want just an "ordinary" file stream, you can
 * happily pass NULL instead and the defaults from the configuration file (or
 * hard-wired defaults if no config file has been read) will be used.
 */
struct fb_params {
  enum fb_type type;			/* The chosen back-end */
  uns buffer_size;			/* 0 for default size */
  uns keep_back_buf;			/* FB_STD: optimize for bi-directional access */
  uns read_ahead;			/* FB_DIRECT options */
  uns write_back;
  struct asio_queue *asio;
};

struct cf_section;
extern struct cf_section fbpar_cf; 	/** Configuration section with which you can fill the `fb_params` **/
extern struct fb_params fbpar_def;	/** The default `fb_params` **/

/**
 * Opens a file with file mode @mode (see the man page of open()).
 * Use @params to select the fastbuf back-end and its parameters or
 * pass NULL if you are fine with defaults.
 *
 * Dies if the file does not exist.
 **/
struct fastbuf *bopen_file(const char *name, int mode, struct fb_params *params);
struct fastbuf *bopen_file_try(const char *name, int mode, struct fb_params *params); /** Like bopen_file(), but returns NULL on failure. **/

/**
 * Opens a temporary file.
 * It is placed with other temp files and it is deleted when closed.
 * Again, use NULL for @params if you want the defaults.
 **/
struct fastbuf *bopen_tmp_file(struct fb_params *params);

/**
 * Creates a fastbuf from a file descriptor @fd and sets its filename
 * to @name (the name is used only in error messages).
 * When the fastbuf is closed, the fd is closed as well. You can override
 * this behavior by calling @bconfig().
 */
struct fastbuf *bopen_fd_name(int fd, struct fb_params *params, const char *name);
static inline struct fastbuf *bopen_fd(int fd, struct fb_params *params) /** Same as above, but with an auto-generated filename. **/
{
  return bopen_fd_name(fd, params, NULL);
}

/**
 * Flushes all buffers and makes sure that they are written to the disk.
 **/
void bfilesync(struct fastbuf *b);

/***
 * === Fastbufs on regular files [[fbfile]]
 *
 * If you want to use the `FB_STD` back-end and not worry about setting
 * up any parameters, there is a couple of shortcuts.
 ***/

struct fastbuf *bopen(const char *name, uns mode, uns buflen);		/** Equivalent to @bopen_file() with `FB_STD` back-end. **/
struct fastbuf *bopen_try(const char *name, uns mode, uns buflen);	/** Equivalent to @bopen_file_try() with `FB_STD` back-end. **/
struct fastbuf *bopen_tmp(uns buflen);					/** Equivalent to @bopen_tmp_file() with `FB_STD` back-end. **/
struct fastbuf *bfdopen(int fd, uns buflen);				/** Equivalent to @bopen_fd() with `FB_STD` back-end. **/
struct fastbuf *bfdopen_shared(int fd, uns buflen);			/** Like @bfdopen(), but it does not close the @fd on @bclose(). **/

/***
 * === Temporary files [[fbtemp]]
 *
 * Usually, @bopen_tmp_file() is the best way how to come to a temporary file.
 * However, in some specific cases you can need more, so there is also a set
 * of more general functions.
 ***/

#define TEMP_FILE_NAME_LEN 256

/**
 * Generates a temporary filename and stores it to the @name_buf (of size
 * at least * `TEMP_FILE_NAME_LEN`). If @open_flags are not NULL, flags that
 * should be OR-ed with other flags to open() will be stored there.
 *
 * The location and style of temporary files is controlled by the configuration.
 * By default, the system temp directory (`$TMPDIR` or `/tmp`) is used.
 *
 * If the location is a publicly writeable directory (like `/tmp`), the
 * generated filename cannot be guaranteed to be unique, so @open_flags
 * will include `O_EXCL` and you have to check the result of open() and
 * iterate if needed.
 *
 * This function is not specific to fastbufs, it can be used separately.
 **/
void temp_file_name(char *name_buf, int *open_flags);

/**
 * Opens a temporary file and returns its file descriptor.
 * You specify the file @mode and @open_flags passed to open().
 *
 * If the @name_buf (of at last `TEMP_FILE_NAME_LEN` chars) is not NULL,
 * the filename is also stored in it.
 *
 * This function is not specific to fastbufs, it can be used separately.
 */
int open_tmp(char *name_buf, int open_flags, int mode);

/**
 * Sometimes, a file is created as temporary and then moved to a stable
 * location. This function takes a fastbuf created by @bopen_tmp_file()
 * or @bopen_tmp(), marks it as permanent, closes it and renames it to
 * @name.
 *
 * Please note that it assumes that the temporary file and the @name
 * are on the same volume (otherwise, rename() fails), so you might
 * want to configure a special location for the temporary files
 * beforehand.
 */
void bfix_tmp_file(struct fastbuf *fb, const char *name);

/* Internal functions of some file back-ends */

struct fastbuf *bfdopen_internal(int fd, const char *name, uns buflen);
struct fastbuf *bfmmopen_internal(int fd, const char *name, uns mode);

extern uns fbdir_cheat;
struct asio_queue;
struct fastbuf *fbdir_open_fd_internal(int fd, const char *name, struct asio_queue *io_queue, uns buffer_size, uns read_ahead, uns write_back);

void bclose_file_helper(struct fastbuf *f, int fd, int is_temp_file);

/***
 * === Fastbufs on file fragments [[fblim]]
 *
 * The `fblim` back-end reads from a file handle, but at most a given
 * number of bytes. This is frequently used for reading from sockets.
 ***/

struct fastbuf *bopen_limited_fd(int fd, uns bufsize, uns limit); /** Create a fastbuf which reads at most @limit bytes from @fd. **/

/***
 * === Fastbufs on in-memory streams [[fbmem]]
 *
 * The `fbmem` back-end keeps the whole contents of the stream
 * in memory (as a linked list of memory blocks, so address space
 * fragmentation is avoided).
 *
 * First, you use @fbmem_create() to create the stream and the fastbuf
 * used for writing to it. Then you can call @fbmem_clone_read() to get
 * an arbitrary number of fastbuf for reading from the stream.
 ***/

struct fastbuf *fbmem_create(uns blocksize);		/** Create stream and return its writing fastbuf. **/
struct fastbuf *fbmem_clone_read(struct fastbuf *f);	/** Given a writing fastbuf, create a new reading fastbuf. **/

/***
 * === Fastbufs on static buffers [[fbbuf]]
 *
 * The `fbbuf` back-end stores the stream in a given block of memory.
 * This is useful for parsing and generating of complex data structures.
 ***/

/**
 * Creates a read-only fastbuf that takes its data from a given buffer.
 * The fastbuf structure is allocated by the caller and pointed to by @f.
 * The @buffer and @size specify the location and size of the buffer.
 *
 * In some cases, the front-ends can take advantage of rewriting the contents
 * of the buffer temporarily. In this case, set @can_overwrite as described
 * in <<internal,Internals>>. If you do not care, keep @can_overwrite zero.
 *
 * It is not possible to close this fastbuf.
 */
void fbbuf_init_read(struct fastbuf *f, byte *buffer, uns size, uns can_overwrite);

/**
 * Creates a write-only fastbuf which writes into a provided memory buffer.
 * The fastbuf structure is allocated by the caller and pointed to by @f.
 * An attempt to write behind the end of the buffer dies.
 *
 * Data are written directly into the buffer, so it is not necessary to call @bflush()
 * at any moment.
 *
 * It is not possible to close this fastbuf.
 */
void fbbuf_init_write(struct fastbuf *f, byte *buffer, uns size);

static inline uns fbbuf_count_written(struct fastbuf *f) /** Calculates, how many bytes were already written into the buffer. **/
{
  return f->bptr - f->bstop;
}

/***
 * === Fastbuf on recyclable growing buffers [[fbgrow]]
 *
 * The `fbgrow` back-end keeps the stream in a contiguous buffer stored in the
 * main memory, but unlike <<fbmem,`fbmem`>>, the buffer does not have a fixed
 * size and it is expanded to accomodate all data.
 *
 * At every moment, you can use `fastbuf->buffer` to gain access to the stream.
 ***/

struct fastbuf *fbgrow_create(unsigned basic_size);	/** Create the growing buffer pre-allocated to @basic_size bytes. **/
void fbgrow_reset(struct fastbuf *b);			/** Reset stream and prepare for writing. **/
void fbgrow_rewind(struct fastbuf *b);			/** Prepare for reading (of already written data). **/

/***
 * === Fastbuf on memory pools [[fbpool]]
 *
 * The write-only `fbpool` back-end also keeps the stream in a contiguous
 * buffer, but this time the buffer is allocated from within a memory pool.
 ***/

struct mempool;
struct fbpool { /** Structure for fastbufs & mempools. **/
  struct fastbuf fb;
  struct mempool *mp;
};

/**
 * Initialize a new `fbpool`. The structure is allocated by the caller.
 **/
void fbpool_init(struct fbpool *fb);	/** Initialize a new mempool fastbuf. **/
/**
 * Start a new continuous block and prepare for writing (see <<mempool:mp_start()>>).
 * Provide the memory pool you want to use for this block as @mp.
 **/
void fbpool_start(struct fbpool *fb, struct mempool *mp, uns init_size);
/**
 * Close the block and return the address of its start (see <<mempool:mp_end()>>).
 * The length can be determined by calling <<mempool:mp_size(mp, ptr)>>.
 **/
void *fbpool_end(struct fbpool *fb);

/***
 * === Atomic files for multi-threaded programs [[fbatomic]]
 *
 * This fastbuf backend is designed for cases when several threads
 * of a single program append records to a common file and while the
 * record can mix in an arbitrary way, the bytes inside a single
 * record must remain uninterrupted.
 *
 * In case of files with fixed record size, we just allocate the
 * buffer to hold a whole number of records and take advantage
 * of the atomicity of the write() system call.
 *
 * With variable-sized records, we need another solution: when
 * writing a record, we keep the fastbuf in a locked state, which
 * prevents buffer flushing (and if the buffer becomes full, we extend it),
 * and we wait for an explicit commit operation which write()s the buffer
 * if the free space in the buffer falls below the expected maximum record
 * length.
 *
 * Please note that initialization of the clones is not thread-safe,
 * so you have to serialize it yourself.
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
 * If @master is NULL, the file @name is opened. If it is non-null,
 * a new clone of an existing atomic fastbuf is created.
 *
 * If the file has fixed record length, just set @record_len to it.
 * Otherwise set @record_len to the expected maximum record length
 * with a negative sign (you need not fit in this length, but as long
 * as you do, the fastbuf is more efficient) and call @fbatomic_commit()
 * after each record.
 *
 * You can specify @record_len, if it is known (for optimisations).
 *
 * The file is closed when all fastbufs using it are closed.
 **/
struct fastbuf *fbatomic_open(const char *name, struct fastbuf *master, uns bufsize, int record_len);
void fbatomic_internal_write(struct fastbuf *b);

/**
 * Declare that you have finished writing a record. This is required only
 * if a fixed record size was not specified.
 **/
static inline void fbatomic_commit(struct fastbuf *b)
{
  if (b->bptr >= ((struct fb_atomic *)b)->expected_max_bptr)
    fbatomic_internal_write(b);
}

/*** === Configuring stream parameters [[bconfig]] ***/

enum bconfig_type {			/** Parameters that could be configured. **/
  BCONFIG_IS_TEMP_FILE,			/* 0=normal file, 1=temporary file, 2=shared fd */
  BCONFIG_KEEP_BACK_BUF,		/* Optimize for bi-directional access */
};

int bconfig(struct fastbuf *f, uns type, int data); /** Configure a fastbuf. Returns previous value. **/

/*** === Universal functions working on all fastbuf's [[ffbasic]] ***/

/**
 * Close and free fastbuf.
 * Can not be used for fastbufs not returned from function (initialized in a parameter, for example the one from `fbbuf_init_read`).
 */
void bclose(struct fastbuf *f);
void bflush(struct fastbuf *f);					/** Write data (if it makes any sense, do not use for in-memory buffers). **/
void bseek(struct fastbuf *f, ucw_off_t pos, int whence);	/** Seek in the buffer. See `man fseek` for description of @whence. Only for seekable fastbufs. **/
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
 * Reads a line into @b and strips trailing `\n`.
 * Returns pointer to the terminating 0 or NULL on `EOF`.
 * Dies if the line is longer than @l.
 **/
char *bgets(struct fastbuf *f, char *b, uns l);
char *bgets0(struct fastbuf *f, char *b, uns l);	/** The same as @bgets(), but for 0-terminated strings. **/
/**
 * Returns either length of read string (excluding the terminator) or -1 if it is too long.
 * In such cases exactly @l bytes are read.
 */
int bgets_nodie(struct fastbuf *f, char *b, uns l);

struct mempool;
struct bb_t;
/**
 * Read a string, strip the trailing `\n` and store it into growing buffer @b.
 * Dies if the line is longer than @limit.
 **/
uns bgets_bb(struct fastbuf *f, struct bb_t *b, uns limit);
/**
 * Read a string, strip the trailing `\n` and store it into buffer allocated from a memory pool.
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
 * Read a string, strip the trailing `\n` and store it on the stack (allocated using alloca()).
 **/
#define bgets_stk(fb) \
  ({ struct bgets_stk_struct _s; _s.f = (fb); for (bgets_stk_init(&_s); _s.cur_len; _s.cur_buf = alloca(_s.cur_len), bgets_stk_step(&_s)); _s.cur_buf; })

/**
 * Write a string, without 0 or `\n` at the end.
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
 * `~0U` means all data, even if more than `~0U` bytes remain.
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
