/*
 *	UCW Library -- Fast Buffered I/O
 *
 *	(c) 1997--2011 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *	(c) 2014 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_FASTBUF_H
#define _UCW_FASTBUF_H

#include <string.h>
#include <alloca.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define bbcopy_slow ucw_bbcopy_slow
#define bclose ucw_bclose
#define bclose_file_helper ucw_bclose_file_helper
#define bconfig ucw_bconfig
#define beof_slow ucw_beof_slow
#define bfdopen ucw_bfdopen
#define bfdopen_internal ucw_bfdopen_internal
#define bfdopen_shared ucw_bfdopen_shared
#define bfilesize ucw_bfilesize
#define bfilesync ucw_bfilesync
#define bfix_tmp_file ucw_bfix_tmp_file
#define bflush ucw_bflush
#define bfmmopen_internal ucw_bfmmopen_internal
#define bgetc_slow ucw_bgetc_slow
#define bgets ucw_bgets
#define bgets0 ucw_bgets0
#define bgets_bb ucw_bgets_bb
#define bgets_mp ucw_bgets_mp
#define bgets_nodie ucw_bgets_nodie
#define bgets_stk_init ucw_bgets_stk_init
#define bgets_stk_step ucw_bgets_stk_step
#define bopen ucw_bopen
#define bopen_fd_name ucw_bopen_fd_name
#define bopen_file ucw_bopen_file
#define bopen_file_try ucw_bopen_file_try
#define bopen_limited_fd ucw_bopen_limited_fd
#define bopen_tmp ucw_bopen_tmp
#define bopen_tmp_file ucw_bopen_tmp_file
#define bopen_try ucw_bopen_try
#define bpeekc_slow ucw_bpeekc_slow
#define bprintf ucw_bprintf
#define bputc_slow ucw_bputc_slow
#define bread_slow ucw_bread_slow
#define brefill ucw_brefill
#define brewind ucw_brewind
#define bseek ucw_bseek
#define bsetpos ucw_bsetpos
#define bskip_slow ucw_bskip_slow
#define bspout ucw_bspout
#define bthrow ucw_bthrow
#define bwrite_slow ucw_bwrite_slow
#define fb_tie ucw_fb_tie
#define fbatomic_internal_write ucw_fbatomic_internal_write
#define fbatomic_open ucw_fbatomic_open
#define fbbuf_init_read ucw_fbbuf_init_read
#define fbbuf_init_write ucw_fbbuf_init_write
#define fbdir_cheat ucw_fbdir_cheat
#define fbdir_open_fd_internal ucw_fbdir_open_fd_internal
#define fbgrow_create ucw_fbgrow_create
#define fbgrow_create_mp ucw_fbgrow_create_mp
#define fbgrow_get_buf ucw_fbgrow_get_buf
#define fbgrow_reset ucw_fbgrow_reset
#define fbgrow_rewind ucw_fbgrow_rewind
#define fbmem_clone_read ucw_fbmem_clone_read
#define fbmem_create ucw_fbmem_create
#define fbmulti_append ucw_fbmulti_append
#define fbmulti_create ucw_fbmulti_create
#define fbmulti_remove ucw_fbmulti_remove
#define fbnull_open ucw_fbnull_open
#define fbnull_start ucw_fbnull_start
#define fbnull_test ucw_fbnull_test
#define fbpar_cf ucw_fbpar_cf
#define fbpar_def ucw_fbpar_def
#define fbpool_end ucw_fbpool_end
#define fbpool_init ucw_fbpool_init
#define fbpool_start ucw_fbpool_start
#define open_tmp ucw_open_tmp
#define temp_file_name ucw_temp_file_name
#define vbprintf ucw_vbprintf
#endif

/***
 * === Internal structure [[internal]]
 *
 * Generally speaking, a fastbuf consists of a buffer and a set of callbacks.
 * All front-end functions operate on the buffer and if the buffer becomes
 * empty or fills up, they ask the corresponding callback to handle the
 * situation. Back-ends then differ just in the definition of the callbacks.
 *
 * The state of the fastbuf is represented by a <<struct_fastbuf,`struct fastbuf`>>,
 * which is a simple structure describing the state of the buffer (the pointers
 * `buffer`, `bufend`), the front-end cursor (`bptr`), the back-end cursor (`bstop`),
 * position of the back-end cursor in the file (`pos`), some flags (`flags`)
 * and pointers to the callback functions.
 *
 * The buffer can be in one of the following states:
 *
 * 1. Flushed:
 *
 *    +------------------------------------+---------------------------+
 *    | unused                             | free space                |
 *    +------------------------------------+---------------------------+
 *    ^              ^                     ^                           ^
 *    buffer      <= bstop (BE pos)     <= bptr (FE pos)            <= bufend
 *
 *   * This schema describes a fastbuf after its initialization or @bflush().
 *   * There is no cached data and we are ready for any read or write operation
 *     (well, only if the back-end supports it).
 *   * The interval `[bptr, bufend]` can be used by front-ends
 *     for writing. If it is empty, the `spout` callback gets called
 *     upon the first write attempt to allocate a new buffer. Otherwise
 *     the fastbuf silently comes to the writing mode.
 *   * When a front-end needs to read something, it calls the `refill` callback.
 *   * The pointers can be either all non-`NULL` or all NULL.
 *   * `bstop == bptr` in most back-ends, but it is not necessary. Some
 *     in-memory streams take advantage of this.
 *
 * 2. Reading:
 *
 *    +------------------------------------+---------------------------+
 *    | read data                          | unused                    |
 *    +------------------------------------+---------------------------+
 *    ^               ^                    ^                           ^
 *    buffer       <= bptr (FE pos)     <= bstop (BE pos)           <= bufend
 *
 *   * If we try to read something, we get to the reading mode.
 *   * No writing is allowed until a flush operation. But note that @bflush()
 *     will simply set `bptr` to `bstop` before `spout`
 *     and it breaks the position of the front-end's cursor,
 *     so the user should seek afwards.
 *   * The interval `[buffer, bstop]` contains a block of data read by the back-end.
 *     `bptr` is the front-end's cursor which points to the next character to be read.
 *     After the last character is read, `bptr == bstop` and the `refill` callback
 *     gets called upon the next read attempt to bring further data.
 *     This gives us an easy way how to implement @bungetc().
 *
 * 3. Writing:
 *
 *    +-----------------------+----------------+-----------------------+
 *    | unused                | written data   | free space            |
 *    +-----------------------+----------------+-----------------------+
 *    ^            ^                           ^                       ^
 *    buffer    <= bstop (BE pos)            < bptr (FE pos)        <= bufend
 *
 *   * This schema corresponds to the situation after a write attempt.
 *   * No reading is allowed until a flush operation.
 *   * The `bptr` points at the position where the next character
 *     will be written to. When we want to write, but `bptr == bufend`, we call
 *     the `spout` hook to flush the witten data and get an empty buffer.
 *   * `bstop` usually points at the beginning of the written data,
 *     but it is not necessary.
 *
 *
 * Rules for back-ends:
 *
 *   - Front-ends are only allowed to change the value of `bptr`, some flags
 *     and if a fatal error occurs, then also `bstop`. Back-ends can rely on it.
 *   - `buffer <= bstop <= bufend` and `buffer <= bptr <= bufend`.
 *   - `pos` should be the real position in the file corresponding to the location of `bstop` in the buffer.
 *     It can be modified by any back-end's callback, but the position of `bptr` (`pos + (bptr - bstop)`)
 *     must stay unchanged after `refill` or `spout`.
 *   - Failed callbacks (except `close`) should use @bthrow().
 *   - Any callback pointer may be NULL in case the callback is not implemented.
 *   - Callbacks can change not only `bptr` and `bstop`, but also the location and size of the buffer;
 *     the fb-mem back-end takes advantage of it.
 *
 *   - Initialization:
 *     * out: `buffer <= bstop <= bptr <= bufend` (flushed).
 *     * @fb_tie() should be called on the newly created fastbuf.
 *
 *   - `refill`:
 *     * in: `buffer <= bstop <= bptr <= bufend` (reading or flushed).
 *     * out: `buffer <= bptr <= bstop <= bufend` (reading).
 *     * Resulting `bptr == bstop` signals the end of file.
 *       The next reading attempt will again call `refill` which can succeed this time.
 *     * The callback must also return zero on EOF (iff `bptr == bstop`).
 *
 *   - `spout`:
 *     * in: `buffer <= bstop <= bptr <= bufend` (writing or flushed).
 *     * out: `buffer <= bstop <= bptr < bufend` (flushed).
 *
 *   - `seek`:
 *     * in: `buffer <= bstop <= bptr <= bufend` (flushed).
 *     * in: `(ofs >= 0 && whence == SEEK_SET) || (ofs <= 0 && whence == SEEK_END)`.
 *     * out: `buffer <= bstop <= bptr <= bufend` (flushed).
 *
 *   - `close`:
 *     * in: `buffer <= bstop <= bptr <= bufend` (flushed or after @bthrow()).
 *     * `close` must always free all internal structures, even when it throws an exception.
 ***/

/**
 * This structure contains the state of the fastbuf. See the discussion above
 * for how it works.
 **/
struct fastbuf {
  byte *bptr, *bstop;				/* State of the buffer */
  byte *buffer, *bufend;			/* Start and end of the buffer */
  char *name;					/* File name (used for error messages) */
  ucw_off_t pos;				/* Position of bstop in the file */
  uint flags;					/* See enum fb_flags */
  int (*refill)(struct fastbuf *);		/* Get a buffer with new data, returns 0 on EOF */
  void (*spout)(struct fastbuf *);		/* Write buffer data to the file */
  int (*seek)(struct fastbuf *, ucw_off_t, int);/* Slow path for @bseek(), buffer already flushed; returns success */
  void (*close)(struct fastbuf *);		/* Close the stream */
  int (*config)(struct fastbuf *, uint, int);	/* Configure the stream */
  int can_overwrite_buffer;			/* Can the buffer be altered? 0=never, 1=temporarily, 2=permanently */
  struct resource *res;				/* The fastbuf can be tied to a resource pool */
};

/**
 * Fastbuf flags
 */
enum fb_flags {
  FB_DEAD = 0x1,				/* Some fastbuf's method has thrown an exception */
  FB_DIE_ON_EOF = 0x2,				/* Most of read operations throw "fb.eof" on EOF */
};

/** Tie a fastbuf to a resource in the current resource pool. Returns the pointer to the same fastbuf. **/
struct fastbuf *fb_tie(struct fastbuf *b);	/* Tie fastbuf to a resource if there is an active pool */

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
  uint buffer_size;			/* 0 for default size */
  uint keep_back_buf;			/* FB_STD: optimize for bi-directional access */
  uint read_ahead;			/* FB_DIRECT options */
  uint write_back;
  struct asio_queue *asio;
};

struct cf_section;
extern struct cf_section fbpar_cf;	/** Configuration section with which you can fill the `fb_params` **/
extern struct fb_params fbpar_def;	/** The default `fb_params` **/

/**
 * Opens a file with file mode @mode (see the man page of open()).
 * Use @params to select the fastbuf back-end and its parameters or
 * pass NULL if you are fine with defaults.
 *
 * Raises `ucw.fb.open` if the file does not exist.
 **/
struct fastbuf *bopen_file(const char *name, int mode, struct fb_params *params);
struct fastbuf *bopen_file_try(const char *name, int mode, struct fb_params *params); /** Like @bopen_file(), but returns NULL on failure. **/

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

struct fastbuf *bopen(const char *name, uint mode, uint buflen);	/** Equivalent to @bopen_file() with `FB_STD` back-end. **/
struct fastbuf *bopen_try(const char *name, uint mode, uint buflen);	/** Equivalent to @bopen_file_try() with `FB_STD` back-end. **/
struct fastbuf *bopen_tmp(uint buflen);					/** Equivalent to @bopen_tmp_file() with `FB_STD` back-end. **/
struct fastbuf *bfdopen(int fd, uint buflen);				/** Equivalent to @bopen_fd() with `FB_STD` back-end. **/
struct fastbuf *bfdopen_shared(int fd, uint buflen);			/** Like @bfdopen(), but it does not close the @fd on @bclose(). **/

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

struct fastbuf *bfdopen_internal(int fd, const char *name, uint buflen);
struct fastbuf *bfmmopen_internal(int fd, const char *name, uint mode);

#ifdef CONFIG_UCW_FB_DIRECT
extern uint fbdir_cheat;
struct asio_queue;
struct fastbuf *fbdir_open_fd_internal(int fd, const char *name, struct asio_queue *io_queue, uint buffer_size, uint read_ahead, uint write_back);
#endif

void bclose_file_helper(struct fastbuf *f, int fd, int is_temp_file);

/***
 * === Fastbufs on file fragments [[fblim]]
 *
 * The `fblim` back-end reads from a file handle, but at most a given
 * number of bytes. This is frequently used for reading from sockets.
 ***/

struct fastbuf *bopen_limited_fd(int fd, uint bufsize, uint limit); /** Create a fastbuf which reads at most @limit bytes from @fd. **/

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

struct fastbuf *fbmem_create(uint blocksize);		/** Create stream and return its writing fastbuf. **/
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
 * A @bclose() on this fastbuf is allowed and it does nothing.
 */
void fbbuf_init_read(struct fastbuf *f, byte *buffer, uint size, uint can_overwrite);

/**
 * Creates a write-only fastbuf which writes into a provided memory buffer.
 * The fastbuf structure is allocated by the caller and pointed to by @f.
 * An attempt to write behind the end of the buffer causes the `ucw.fb.write` exception.
 *
 * Data are written directly into the buffer, so it is not necessary to call @bflush()
 * at any moment.
 *
 * A @bclose() on this fastbuf is allowed and it does nothing.
 */
void fbbuf_init_write(struct fastbuf *f, byte *buffer, uint size);

static inline uint fbbuf_count_written(struct fastbuf *f) /** Calculates, how many bytes were already written into the buffer. **/
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

struct mempool;

struct fastbuf *fbgrow_create(uint basic_size);		/** Create the growing buffer pre-allocated to @basic_size bytes. **/
struct fastbuf *fbgrow_create_mp(struct mempool *mp, uint basic_size); /** Create the growing buffer pre-allocated to @basic_size bytes. **/
void fbgrow_reset(struct fastbuf *b);			/** Reset stream and prepare for writing. **/
void fbgrow_rewind(struct fastbuf *b);			/** Prepare for reading (of already written data). **/

/**
 * Can be used in any state of @b (for example when writing or after
 * @fbgrow_rewind()) to return the pointer to internal buffer and its length in
 * bytes. The returned buffer can be invalidated by further requests.
 **/
uint fbgrow_get_buf(struct fastbuf *b, byte **buf);

/***
 * === Fastbuf on memory pools [[fbpool]]
 *
 * The write-only `fbpool` back-end also keeps the stream in a contiguous
 * buffer, but this time the buffer is allocated from within a memory pool.
 ***/

struct fbpool { /** Structure for fastbufs & mempools. **/
  struct fastbuf fb;
  struct mempool *mp;
};

/**
 * Initialize a new `fbpool`. The structure is allocated by the caller.
 * Calling @bclose() is optional.
 **/
void fbpool_init(struct fbpool *fb);	/** Initialize a new mempool fastbuf. **/
/**
 * Start a new continuous block and prepare for writing (see <<mempool:mp_start()>>).
 * Provide the memory pool you want to use for this block as @mp.
 **/
void fbpool_start(struct fbpool *fb, struct mempool *mp, size_t init_size);
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
  uint slack_size;
};

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
struct fastbuf *fbatomic_open(const char *name, struct fastbuf *master, uint bufsize, int record_len);
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

/*** === Null fastbufs ***/

/**
 * Creates a new "/dev/null"-like  fastbuf.
 * Any read attempt returns an EOF, any write attempt is silently ignored.
 **/
struct fastbuf *fbnull_open(uint bufsize);

/**
 * Can be used by any back-end to switch it to the null mode.
 * You need to provide at least one byte long buffer for writing.
 **/
void fbnull_start(struct fastbuf *b, byte *buf, uint bufsize);

/**
 * Checks whether a fastbuf has been switched to the null mode.
 **/
bool fbnull_test(struct fastbuf *b);

/***
 * === Fastbufs atop other fastbufs [[fbmulti]]
 *
 * Imagine some code which does massive string processing. It takes an input
 * buffer, writes a part of it into an output buffer, then some other string
 * and then the remaining part of the input buffer. Or anything else where you
 * copy all the data at each stage of the complicated process.
 *
 * This backend takes multiple fastbufs and concatenates them formally into
 * one. You may then read them consecutively as they were one fastbuf at all.
 *
 * This backend is read-only.
 *
 * This backend is seekable iff all of the supplied fastbufs are seekable.
 *
 * You aren't allowed to do anything with the underlying buffers while these
 * are connected into fbmulti.
 *
 * The fbmulti is inited by @fbmulti_create(). It returns an empty fbmulti.
 * Then you call @fbmulti_append() for each fbmulti.
 *
 * If @bclose() is called on fbmulti, all the underlying buffers get closed
 * recursively.
 *
 * If you want to keep an underlying fastbuf open after @bclose, just remove it
 * by @fbmulti_remove where the second parameter is a pointer to the removed
 * fastbuf. If you pass NULL, all the underlying fastbufs are removed.
 *
 * After @fbmulti_remove, the state of the fbmulti is undefined. The only allowed
 * operation is either another @fbmulti_remove or @bclose on the fbmulti.
 ***/

/**
 * Create an empty fbmulti
 **/
struct fastbuf *fbmulti_create(void);

/**
 * Append a fb to fbmulti
 **/
void fbmulti_append(struct fastbuf *f, struct fastbuf *fb);

/**
 * Remove a fb from fbmulti
 **/
void fbmulti_remove(struct fastbuf *f, struct fastbuf *fb);

/*** === Configuring stream parameters [[bconfig]] ***/

enum bconfig_type {			/** Parameters that could be configured. **/
  BCONFIG_IS_TEMP_FILE,			/* 0=normal file, 1=temporary file, 2=shared fd */
  BCONFIG_KEEP_BACK_BUF,		/* Optimize for bi-directional access */
};

int bconfig(struct fastbuf *f, uint type, int data); /** Configure a fastbuf. Returns previous value. **/

/*** === Universal functions working on all fastbuf's [[ffbasic]] ***/

/**
 * Close and free fastbuf.
 * Some kinds of fastbufs are allocated by the caller (e.g., in @fbbuf_init_read());
 * in such cases, @bclose() does not free any memory.
 */
void bclose(struct fastbuf *f);
void bthrow(struct fastbuf *f, const char *id, const char *fmt, ...) FORMAT_CHECK(printf,3,4) NONRET;	/** Throw exception on a given fastbuf **/
int brefill(struct fastbuf *f, int allow_eof);
void bspout(struct fastbuf *f);
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

int beof_slow(struct fastbuf *f);
static inline int beof(struct fastbuf *f)			/** Have I reached EOF? **/
{
  return (f->bptr < f->bstop) ? 0 : beof_slow(f);
}

static inline void bungetc(struct fastbuf *f)			/** Return last read character back. Only one back is guaranteed to work. **/
{
  f->bptr--;
}

void bputc_slow(struct fastbuf *f, uint c);
static inline void bputc(struct fastbuf *f, uint c)		/** Write a single character. **/
{
  if (f->bptr < f->bufend)
    *f->bptr++ = c;
  else
    bputc_slow(f, c);
}

static inline uint bavailr(struct fastbuf *f)			/** Return the length of the cached data to be read. Do not use directly. **/
{
  return f->bstop - f->bptr;
}

static inline uint bavailw(struct fastbuf *f)			/** Return the length of the buffer available for writing. Do not use directly. **/
{
  return f->bufend - f->bptr;
}

uint bread_slow(struct fastbuf *f, void *b, uint l, uint check);
/**
 * Read at most @l bytes of data into @b.
 * Returns number of bytes read.
 * 0 means end of file.
 */
static inline uint bread(struct fastbuf *f, void *b, uint l)
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
 * If there are data, but less than @l, it raises `ucw.fb.eof`.
 */
static inline uint breadb(struct fastbuf *f, void *b, uint l)
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

void bwrite_slow(struct fastbuf *f, const void *b, uint l);
static inline void bwrite(struct fastbuf *f, const void *b, uint l) /** Writes buffer @b of length @l into fastbuf. **/
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
 * Raises `ucw.fb.toolong` if the line is longer than @l.
 **/
char *bgets(struct fastbuf *f, char *b, uint l);
char *bgets0(struct fastbuf *f, char *b, uint l);	/** The same as @bgets(), but for 0-terminated strings. **/
/**
 * Returns either length of read string (excluding the terminator) or -1 if it is too long.
 * In such cases exactly @l bytes are read.
 */
int bgets_nodie(struct fastbuf *f, char *b, uint l);

struct mempool;
struct bb_t;
/**
 * Read a string, strip the trailing `\n` and store it into growing buffer @b.
 * Raises `ucw.fb.toolong` if the line is longer than @limit.
 **/
uint bgets_bb(struct fastbuf *f, struct bb_t *b, uint limit);
/**
 * Read a string, strip the trailing `\n` and store it into buffer allocated from a memory pool.
 **/
char *bgets_mp(struct fastbuf *f, struct mempool *mp);

struct bgets_stk_struct {
  struct fastbuf *f;
  byte *old_buf, *cur_buf, *src;
  uint old_len, cur_len, src_len;
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

void bbcopy_slow(struct fastbuf *f, struct fastbuf *t, uint l);
/**
 * Copy @l bytes of data from fastbuf @f to fastbuf @t.
 * `UINT_MAX` (`~0U`) means all data, even if more than `UINT_MAX` bytes remain.
 **/
static inline void bbcopy(struct fastbuf *f, struct fastbuf *t, uint l)
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

int bskip_slow(struct fastbuf *f, uint len);
static inline int bskip(struct fastbuf *f, uint len) /** Skip @len bytes without reading them. **/
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

/**
 * Begin direct reading from fastbuf's internal buffer to avoid unnecessary copying.
 * The function returns a buffer @buf together with its length in bytes (zero means EOF)
 * with cached data to be read.
 *
 * Some back-ends allow the user to modify the data in the returned buffer to avoid unnecessary.
 * If the back-end allows such modifications, it can set `f->can_overwrite_buffer` accordingly:
 *
 *   - 0 if no modification is allowed,
 *   - 1 if the user can modify the buffer on the condition that
 *       the modifications will be undone before calling the next
 *       fastbuf operation
 *   - 2 if the user is allowed to overwrite the data in the buffer
 *       if @bdirect_read_commit_modified() is called afterwards.
 *       In this case, the back-end must be prepared for trimming
 *       of the buffer which is done by the commit function.
 *
 * The reading must be ended by @bdirect_read_commit() or @bdirect_read_commit_modified(),
 * unless the user did not read or modify anything.
 **/
static inline uint bdirect_read_prepare(struct fastbuf *f, byte **buf)
{
  if (f->bptr == f->bstop && !f->refill(f))
    {
      *buf = NULL;  // This is not needed, but it helps to get rid of spurious warnings
      return 0;
    }
  *buf = f->bptr;
  return bavailr(f);
}

/**
 * End direct reading started by @bdirect_read_prepare() and move the cursor at @pos.
 * Data in the returned buffer must be same as after @bdirect_read_prepare() and
 * @pos must point somewhere inside the buffer.
 **/
static inline void bdirect_read_commit(struct fastbuf *f, byte *pos)
{
  f->bptr = pos;
}

/**
 * Similar to @bdirect_read_commit(), but accepts also modified data before @pos.
 * Note that such modifications are supported only if `f->can_overwrite_buffer == 2`.
 **/
static inline void bdirect_read_commit_modified(struct fastbuf *f, byte *pos)
{
  f->bptr = pos;
  f->buffer = pos;	/* Avoid seeking backwards in the buffer */
}

/**
 * Start direct writing to fastbuf's internal buffer to avoid copy overhead.
 * The function returns the length of the buffer in @buf (at least one byte)
 * where we can write to. The operation must be ended by @bdirect_write_commit(),
 * unless nothing is written.
 **/
static inline uint bdirect_write_prepare(struct fastbuf *f, byte **buf)
{
  if (f->bptr == f->bufend)
    f->spout(f);
  *buf = f->bptr;
  return bavailw(f);
}

/**
 * Commit the data written to the buffer returned by @bdirect_write_prepare().
 * The length is specified by @pos which must point just after the written data.
 * Also moves the cursor to @pos.
 **/
static inline void bdirect_write_commit(struct fastbuf *f, byte *pos)
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
