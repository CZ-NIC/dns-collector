/*
 *	UCW Library -- Main Loop
 *
 *	(c) 2004--2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_MAINLOOP_H
#define _UCW_MAINLOOP_H

#include <ucw/clists.h>
#include <ucw/process.h>

#include <signal.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define block_io_add ucw_block_io_add
#define block_io_del ucw_block_io_del
#define block_io_read ucw_block_io_read
#define block_io_set_timeout ucw_block_io_set_timeout
#define block_io_write ucw_block_io_write
#define file_add ucw_file_add
#define file_chg ucw_file_chg
#define file_debug ucw_file_debug
#define file_del ucw_file_del
#define hook_add ucw_hook_add
#define hook_debug ucw_hook_debug
#define hook_del ucw_hook_del
#define main_cleanup ucw_main_cleanup
#define main_current ucw_main_current
#define main_debug_context ucw_main_debug_context
#define main_delete ucw_main_delete
#define main_destroy ucw_main_destroy
#define main_get_time ucw_main_get_time
#define main_init ucw_main_init
#define main_loop ucw_main_loop
#define main_new ucw_main_new
#define main_step ucw_main_step
#define main_switch_context ucw_main_switch_context
#define main_teardown ucw_main_teardown
#define process_add ucw_process_add
#define process_debug ucw_process_debug
#define process_del ucw_process_del
#define process_fork ucw_process_fork
#define rec_io_add ucw_rec_io_add
#define rec_io_del ucw_rec_io_del
#define rec_io_parse_line ucw_rec_io_parse_line
#define rec_io_set_timeout ucw_rec_io_set_timeout
#define rec_io_start_read ucw_rec_io_start_read
#define rec_io_stop_read ucw_rec_io_stop_read
#define rec_io_write ucw_rec_io_write
#define signal_add ucw_signal_add
#define signal_debug ucw_signal_debug
#define signal_del ucw_signal_del
#define timer_add ucw_timer_add
#define timer_add_rel ucw_timer_add_rel
#define timer_debug ucw_timer_debug
#define timer_del ucw_timer_del
#endif

/***
 * [[basic]]
 * Basic operations
 * ----------------
 *
 * First of all, let us take a look at the basic operations with main loop contexts.
 ***/

/** The main loop context **/
struct main_context {
  timestamp_t now;			/* [*] Current time in milliseconds since an unknown epoch. See main_get_time(). */
  timestamp_t idle_time;		/* [*] Total time in milliseconds spent by waiting for events. */
  uint shutdown;				/* [*] Setting this to nonzero forces the main_loop() function to terminate. */
  clist file_list;
  clist file_active_list;
  clist hook_list;
  clist hook_done_list;
  clist process_list;
  clist signal_list;
  uint file_cnt;
  uint single_step;
#ifdef CONFIG_UCW_EPOLL
  int epoll_fd;				/* File descriptor used for epoll */
  struct epoll_event *epoll_events;
  clist file_recalc_list;
#else
  uint poll_table_obsolete;
  struct pollfd *poll_table;
  struct main_file **poll_file_table;
#endif
  uint poll_cnt;
  struct main_timer **timer_table;	/* Growing array containing the heap of timers */
  sigset_t want_signals;
  int sig_pipe_send;
  int sig_pipe_recv;
  struct main_file *sig_pipe_file;
  struct main_signal *sigchld_handler;
};

struct main_context *main_new(void);		/** Create a new context. **/

/**
 * Delete a context, assuming it does have any event handlers attached. Does nothing if @m is NULL.
 * It is allowed to call @main_delete() from a hook function of the same context, but you must
 * never return to the main loop -- e.g., you can exit() the process instead.
 **/
void main_delete(struct main_context *m);

/**
 * Delete a context. If there are any event handlers attached, they are deactivated
 * (but the responsibility to free the memory there were allocated from lies upon you).
 * If there are any file handlers, the corresponding file descriptors are closed.
 **/
void main_destroy(struct main_context *m);

/** Switch the current context of the calling thread. Returns the previous current context. **/
struct main_context *main_switch_context(struct main_context *m);

/** Return the current context. Dies if there is none or if the context has been deleted. **/
struct main_context *main_current(void);

/** Initialize the main loop module and create a top-level context. **/
void main_init(void);

/** Deinitialize the main loop module, calling @main_delete() on the top-level context. **/
void main_cleanup(void);

/**
 * Deinitialize the main loop module, calling @main_destroy() on the top-level context.
 * This is especially useful in a freshly forked-off child process.
 **/
void main_teardown(void);

/**
 * Start the event loop on the current context.
 * It will watch the provided objects and call callbacks.
 * Terminates when someone calls @main_shut_down(),
 * or when all <<hook,hooks>> return <<enum_main_hook_return,`HOOK_DONE`>>
 * or at last one <<hook,hook>> returns <<enum_main_hook_return,`HOOK_SHUTDOWN`>>.
 **/
void main_loop(void);

/**
 * Perform a single iteration of the main loop.
 * Check if there are any events ready and process them.
 * If there are none, do not wait.
 **/
void main_step(void);

/** Ask the main loop to terminate at the nearest occasion. **/
static inline void main_shut_down(void)
{
  main_current()->shutdown = 1;
}

/**
 * Show the current state of a given context (use @main_debug() for the current context).
 * Available only if LibUCW has been compiled with `CONFIG_UCW_DEBUG`.
 **/
void main_debug_context(struct main_context *m);

static inline void main_debug(void)
{
  main_debug_context(main_current());
}

/***
 * [[time]]
 * Timers
 * ------
 *
 * The event loop provides the current time, measured as a 64-bit number
 * of milliseconds since the system epoch (represented in the type `timestamp_t`).
 *
 * You can also register timers, which call a handler function at a given moment.
 * The handler function must either call @timer_del() to delete the timer, or call
 * @timer_add() with a different expiration time.
 ***/

/**
 * Get the current timestamp cached in the current context. It is refreshed in every
 * iteration of the event loop, or explicitly by calling @main_get_time().
 **/
static inline timestamp_t main_get_now(void)
{
  return main_current()->now;
}

/**
 * This is a description of a timer.
 * You define the handler function and possibly user-defined data you wish
 * to pass to the handler, and then you invoke @timer_add().
 **/
struct main_timer {
  cnode n;
  timestamp_t expires;
  uint index;
  void (*handler)(struct main_timer *tm);	/* [*] Function to be called when the timer expires. */
  void *data;					/* [*] Data for use by the handler */
};

/**
 * Add a new timer into the main loop to be watched and called
 * when it expires. It can also be used to modify an already running
 * timer. It is permitted (and usual) to call this function from the
 * timer's handler itself if you want the timer to trigger again.
 *
 * The @expire parameter is absolute (in the same time scale as @main_get_now()),
 * use @timer_add_rel() for a relative version.
 **/
void timer_add(struct main_timer *tm, timestamp_t expires);

/** Like @timer_add(), but the expiration time is relative to the current time. **/
void timer_add_rel(struct main_timer *tm, timestamp_t expires_delta);

/**
 * Removes a timer from the active ones. It is permitted (and common) to call
 * this function from the timer's handler itself if you want to deactivate
 * the timer. Removing an already removed timer does nothing.
 **/
void timer_del(struct main_timer *tm);

/** Tells whether a timer is running. **/
static inline int timer_is_active(struct main_timer *tm)
{
  return !!tm->expires;
}

/**
 * Forces refresh of the current timestamp cached in the active context.
 * You usually do not need to call this, since it is called every time the
 * loop polls for events. It is here if you need extra precision or some of the
 * hooks takes a long time.
 **/
void main_get_time(void);

/** Show current state of a timer. Available only if LibUCW has been compiled with `CONFIG_UCW_DEBUG`. **/
void timer_debug(struct main_timer *tm);

/***
 * [[hooks]]
 * Loop hooks
 * ----------
 *
 * The hooks are called whenever the main loop performs an iteration.
 * You can shutdown the main loop from within them or request an iteration
 * to happen without sleeping (just poll, no waiting for events).
 ***/

/**
 * A hook. It contains the function to call and some user data.
 *
 * The handler() must return one value from
 * <<enum_main_hook_return,`main_hook_return`>>.
 *
 * Fill with the hook and data and pass it to @hook_add().
 **/
struct main_hook {
  cnode n;
  int (*handler)(struct main_hook *ho);		/* [*] Hook function; returns HOOK_xxx */
  void *data;					/* [*] For use by the handler */
};

/**
 * Return value of the hook handler().
 * Specifies what should happen next.
 *
 * - `HOOK_IDLE` -- Let the loop sleep until something happens, call after that.
 * - `HOOK_RETRY` -- Force the loop to perform another iteration without sleeping.
 *   This will cause calling of all the hooks again soon.
 * - `HOOK_DONE` -- The loop will terminate if all hooks return this.
 * - `HOOK_SHUTDOWN` -- Shuts down the loop.
 *
 * The `HOOK_IDLE` and `HOOK_RETRY` constants are also used as return values
 * of file handlers.
 **/
enum main_hook_return {
  HOOK_IDLE,
  HOOK_RETRY,
  HOOK_DONE = -1,
  HOOK_SHUTDOWN = -2
};

/**
 * Inserts a new hook into the loop.
 * The hook will be scheduled at least once before next sleep.
 * May be called from inside a hook handler too.
 * Adding an already added hook is permitted and if the hook has been run,
 * it will be run again before next sleep.
 **/
void hook_add(struct main_hook *ho);

/**
 * Removes an existing hook from the loop.
 * May be called from inside a hook handler (to delete itself or another hook).
 * Removing an already removed hook does nothing.
 **/
void hook_del(struct main_hook *ho);

/** Tells if a hook is active (i.e., added). **/
static inline int hook_is_active(struct main_hook *ho)
{
  return clist_is_linked(&ho->n);
}

/** Show current state of a hook. Available only if LibUCW has been compiled with `CONFIG_UCW_DEBUG`. **/
void hook_debug(struct main_hook *ho);


/***
 * [[file]]
 * Activity on file descriptors
 * ----------------------------
 *
 * You can ask the main loop to watch a set of file descriptors for activity.
 * (This is a generalization of the select() and poll() system calls. Internally,
 * it uses either poll() or the more efficient epoll().)
 *
 * You create a <<struct_main_file,`struct main_file`>>, fill in a file descriptor
 * and pointers to handler functions to be called when the descriptor becomes
 * ready for reading and/or writing, and call @file_add(). When you need to
 * modify the handlers (e.g., to set them to NULL if you are no longer interested
 * in a given event), you should call @file_chg() to notify the main loop about
 * the changes.
 *
 * From within the handler functions, you are allowed to call @file_chg() and even
 * @file_del().
 *
 * The return value of a handler function should be either <<enum_main_hook_return,`HOOK_RETRY`>>
 * or <<enum_main_hook_return,`HOOK_IDLE`>>. <<enum_main_hook_return,`HOOK_RETRY`>>
 * signals that the function would like to consume more data immediately
 * (i.e., it wants to be called again soon, but the event loop can postpone it after
 * processing other events to avoid starvation). <<enum_main_hook_return,`HOOK_IDLE`>>
 * tells that the handler wants to be called when the descriptor becomes ready again.
 *
 * For backward compatibility, 0 can be used instead of <<enum_main_hook_return,`HOOK_IDLE`>>
 * and 1 for <<enum_main_hook_return,`HOOK_RETRY`>>.
 *
 * If you want to read/write fixed-size blocks of data asynchronously, the
 * <<blockio,Asynchronous block I/O>> interface could be more convenient.
 ***/

/**
 * This structure describes a file descriptor to be watched and the handlers
 * to be called when the descriptor is ready for reading and/or writing.
 **/
struct main_file {
  cnode n;
  int fd;					/* [*] File descriptor */
  int (*read_handler)(struct main_file *fi);	/* [*] To be called when ready for reading/writing; must call file_chg() afterwards */
  int (*write_handler)(struct main_file *fi);
  void *data;					/* [*] Data for use by the handlers */
  uint events;
  uint want_events;
#ifndef CONFIG_UCW_EPOLL
  struct pollfd *pollfd;
#endif
};

/**
 * Insert a <<struct_main_file,`main_file`>> structure into the main loop to be
 * watched for activity. You can call this at any time, even inside a handler
 * (of course for a different file descriptor than the one of the handler).
 *
 * The file descriptor is automatically set to the non-blocking mode.
 **/
void file_add(struct main_file *fi);

/**
 * Tell the main loop that the file structure has changed. Call it whenever you
 * change any of the handlers.
 *
 * Can be called only on active files (only the ones added by @file_add()).
 **/
void file_chg(struct main_file *fi);

/**
 * Removes a file from the watched set. If you want to close a descriptor,
 * please use this function first.
 *
 * Can be called from a handler.
 * Removing an already removed file does nothing.
 **/
void file_del(struct main_file *fi);

/** Tells if a file is active (i.e., added). **/
static inline int file_is_active(struct main_file *fi)
{
  return clist_is_linked(&fi->n);
}

/** Show current state of a file. Available only if LibUCW has been compiled with `CONFIG_UCW_DEBUG`. **/
void file_debug(struct main_file *fi);

/***
 * [[blockio]]
 * Asynchronous block I/O
 * ----------------------
 *
 * If you are reading or writing fixed-size blocks of data, you can let the
 * block I/O interface handle the boring routine of handling partial reads
 * and writes for you.
 *
 * You just create <<struct_main_block_io,`struct main_block_io`>> and call
 * @block_io_add() on it, which sets up some <<struct_main_file,`main_file`>>s internally.
 * Then you can just call @block_io_read() or @block_io_write() to ask for
 * reading or writing of a given block. When the operation is finished,
 * your handler function is called.
 *
 * Additionally, the block I/O is equipped with a timer, which can be used
 * to detect communication timeouts. The timer is not touched internally
 * (except that it gets added and deleted at the right places), feel free
 * to adjust it from your handler functions by @block_io_set_timeout().
 * When the timer expires, the error handler is automatically called with
 * <<enum_block_io_err_cause,`BIO_ERR_TIMEOUT`>>.
 ***/

/** The block I/O structure. **/
struct main_block_io {
  struct main_file file;
  byte *rbuf;					/* Read/write pointers for use by file_read/write */
  uint rpos, rlen;
  const byte *wbuf;
  uint wpos, wlen;
  void (*read_done)(struct main_block_io *bio);	/* [*] Called when file_read is finished; rpos < rlen if EOF */
  void (*write_done)(struct main_block_io *bio);	/* [*] Called when file_write is finished */
  void (*error_handler)(struct main_block_io *bio, int cause);	/* [*] Handler to call on errors */
  struct main_timer timer;
  void *data;					/* [*] Data for use by the handlers */
};

/** Activate a block I/O structure. **/
void block_io_add(struct main_block_io *bio, int fd);

/** Deactivate a block I/O structure. Calling twice is safe. **/
void block_io_del(struct main_block_io *bio);

/**
 * Specifies when or why an error happened. This is passed to the error handler.
 * `errno` is still set to the original source of error. The only exception
 * is `BIO_ERR_TIMEOUT`, in which case `errno` is not set and the only possible
 * cause of it is timeout of the timer associated with the block_io
 * (see @block_io_set_timeout()).
 **/
enum block_io_err_cause {
  BIO_ERR_READ,
  BIO_ERR_WRITE,
  BIO_ERR_TIMEOUT
};

/**
 * Ask the main loop to read @len bytes of data from @bio into @buf.
 * It cancels any previous unfinished read requested in this way.
 *
 * When the read is done, the read_done() handler is called. If an EOF occurred,
 * `rpos < rlen` (eg. not all data were read).
 *
 * Can be called from a handler.
 *
 * You can use a call with zero @len to cancel the current read, but all read data
 * will be thrown away.
 **/
void block_io_read(struct main_block_io *bio, void *buf, uint len);

/**
 * Request that the main loop writes @len bytes of data from @buf to @bio.
 * Cancels any previous unfinished write and overwrites `write_handler`.
 *
 * When it is written, the write_done() handler is called.
 *
 * Can be called from a handler.
 *
 * If you call it with zero @len, it will cancel the previous write, but note
 * that some data may already be written.
 **/
void block_io_write(struct main_block_io *bio, const void *buf, uint len);

/**
 * Sets a timer for a file @bio. If the timer is not overwritten or disabled
 * until @expires_delta milliseconds, the file timeouts and error_handler() is called with
 * <<enum_block_io_err_cause,`BIO_ERR_TIMEOUT`>>. A value of `0` stops the timer.
 *
 * Previous setting of the timeout on the same file will be overwritten.
 *
 * The use-cases for this are mainly sockets or pipes, when:
 *
 * - You want to drop inactive connections (no data comes in or out for a given time, not
 *   incomplete messages).
 * - You want to enforce answer in a given time (for example authentication).
 * - Watching maximum time for a whole connection.
 **/
void block_io_set_timeout(struct main_block_io *bio, timestamp_t expires_delta);

/** Tells if a @bio is active (i.e., added). **/
static inline int block_io_is_active(struct main_block_io *bio)
{
  return file_is_active(&bio->file);
}

/***
 * [[recordio]]
 * Asynchronous record I/O
 * -----------------------
 *
 * Record-based I/O is another front-end to the main loop file operations.
 * Unlike its older cousin `main_block_io`, it is able to process records
 * of variable length.
 *
 * To set it up, you create <<struct_main_rec_io,`struct main_rec_io`>> and call
 * @rec_io_add() on it, which sets up some <<struct_main_file,`main_file`>>s internally.
 *
 * To read data from the file, call @rec_io_start_read() first. Whenever any data
 * arrive from the file, they are appended to an internal buffer and the `read_handler`
 * hook is called. The hook checks if the buffer already contains a complete record.
 * If it is so, it processes the record and returns the number of bytes consumed.
 * Otherwise, it returns 0 to tell the buffering machinery that more data are needed.
 * When the read handler decides to destroy the `main_rec_io`, it must return `~0U`.
 *
 * On the write side, `main_rec_io` maintains a buffer keeping all data that should
 * be written to the file. The @rec_io_write() function appends data to this buffer
 * and it is written on background. A simple flow-control mechanism can be asked
 * for: when more than `write_throttle_read` data are buffered for writing, reading
 * is temporarily suspended.
 *
 * Additionally, the record I/O is equipped with a timer, which can be used
 * to detect communication timeouts. The timer is not touched internally
 * (except that it gets added and deleted at the right places), feel free
 * to adjust it from your handler functions by @rec_io_set_timeout().
 *
 * All important events are passed to the `notify_handler`: errors when
 * reading or writing, timeouts, the write buffer becoming empty, ... See
 * <<enum_rec_io_notify_status,`enum rec_io_notify_status`>> for a complete list.
 ***/

/** The record I/O structure. **/
struct main_rec_io {
  struct main_file file;
  byte *read_buf;
  byte *read_rec_start;				/* [*] Start of current record */
  uint read_avail;				/* [*] How much data is available */
  uint read_prev_avail;				/* [*] How much data was available in previous read_handler */
  uint read_buf_size;				/* [*] Read buffer size allocated (can be set before rec_io_add()) */
  uint read_started;				/* Reading requested by user */
  uint read_running;				/* Reading really runs (read_started && not stopped by write_throttle_read) */
  uint read_rec_max;				/* [*] Maximum record size (0=unlimited) */
  clist busy_write_buffers;
  clist idle_write_buffers;
  uint write_buf_size;				/* [*] Write buffer size allocated (can be set before rec_io_add()) */
  uint write_watermark;				/* [*] How much data are waiting to be written */
  uint write_throttle_read;			/* [*] If more than write_throttle_read bytes are buffered, stop reading; 0=no stopping */
  uint (*read_handler)(struct main_rec_io *rio);	/* [*] Called whenever more bytes are read; returns 0 (want more) or number of bytes eaten */
  int (*notify_handler)(struct main_rec_io *rio, int status);	/* [*] Called to notify about errors and other events */
						/* Returns either HOOK_RETRY or HOOK_IDLE. */
  struct main_timer timer;
  struct main_hook start_read_hook;		/* Used internally to defer rec_io_start_read() */
  void *data;					/* [*] Data for use by the handlers */
};

/** Activate a record I/O structure. **/
void rec_io_add(struct main_rec_io *rio, int fd);

/** Deactivate a record I/O structure. Calling twice is safe. **/
void rec_io_del(struct main_rec_io *rio);

/**
 * Start reading.
 *
 * When there were some data in the buffer (e.g., because @rec_io_stop_read()
 * was called from the `read_handler`), it is processed as if it were read
 * from the file once again. That is, `read_prev_avail` is reset to 0 and
 * the `read_handler` is called to process all buffered data.
 ***/
void rec_io_start_read(struct main_rec_io *rio);

/** Stop reading. **/
void rec_io_stop_read(struct main_rec_io *rio);

/** Analogous to @block_io_set_timeout(). **/
void rec_io_set_timeout(struct main_rec_io *rio, timestamp_t expires_delta);

void rec_io_write(struct main_rec_io *rio, const void *data, uint len);

/**
 * An auxiliary function used for parsing of lines. When called in the @read_handler,
 * it searches for the end of line character. When a complete line is found, the length
 * of the line (including the end of line character) is returned. Otherwise, it returns zero.
 **/
uint rec_io_parse_line(struct main_rec_io *rio);

/**
 * Specifies what kind of error or other event happened, when the @notify_handler
 * is called. In case of I/O errors, `errno` is still set.
 *
 * Upon @RIO_ERR_READ, @RIO_ERR_RECORD_TOO_LARGE and @RIO_EVENT_EOF, reading is stopped
 * automatically. Upon @RIO_ERR_WRITE, writing is stopped. Upon @RIO_ERR_TIMEOUT, only the
 * timer is deactivated.
 *
 * In all cases, the notification handler is allowed to call @rec_io_del(), but it
 * must return @HOOK_IDLE in such cases.
 **/
enum rec_io_notify_status {
  RIO_ERR_READ = -1,			/* read() returned an error, errno set */
  RIO_ERR_WRITE = -2,			/* write() returned an error, errno set */
  RIO_ERR_TIMEOUT = -3,			/* A timeout has occurred */
  RIO_ERR_RECORD_TOO_LARGE = -4,	/* Read: read_rec_max has been exceeded */
  RIO_EVENT_ALL_WRITTEN = 1,		/* All buffered data has been written */
  RIO_EVENT_PART_WRITTEN = 2,		/* Some buffered data has been written, but more remains */
  RIO_EVENT_EOF = 3,			/* Read: EOF seen */
};

/** Tells if a @rio is active (i.e., added). **/
static inline int rec_io_is_active(struct main_rec_io *rio)
{
  return file_is_active(&rio->file);
}

/***
 * [[process]]
 * Child processes
 * ---------------
 *
 * The main loop can watch child processes and notify you,
 * when some of them terminates.
 ***/

/**
 * Description of a watched process.
 * You fill in the handler() and `data`.
 * The rest is set with @process_fork().
 **/
struct main_process {
  cnode n;
  int pid;					/* Process id (0=not running) */
  int status;					/* Exit status (-1=fork failed) */
  char status_msg[EXIT_STATUS_MSG_SIZE];
  void (*handler)(struct main_process *mp);	/* [*] Called when the process exits; process_del done automatically */
  void *data;					/* [*] For use by the handler */
};

/**
 * Asks the main loop to watch this process.
 * As it is done automatically in @process_fork(), you need this only
 * if you removed the process previously by @process_del().
 **/
void process_add(struct main_process *mp);

/**
 * Removes the process from the watched set. This is done
 * automatically, when the process terminates, so you need it only
 * when you do not want to watch a running process any more.
 * Removing an already removed process does nothing.
 */
void process_del(struct main_process *mp);

/**
 * Forks and fills the @mp with information about the new process.
 *
 * If the fork() succeeds, it:
 *
 * - Returns 0 in the child.
 * - Returns 1 in the parent and calls @process_add() on it.
 *
 * In the case of unsuccessful fork(), it:
 *
 * - Fills in the `status_msg` and sets `status` to -1.
 * - Calls the handler() as if the process terminated.
 * - Returns 1.
 **/
int process_fork(struct main_process *mp);

/** Tells if a process is active (i.e., added). **/
static inline int process_is_active(struct main_process *mp)
{
  return clist_is_linked(&mp->n);
}

/** Show current state of a process. Available only if LibUCW has been compiled with `CONFIG_UCW_DEBUG`. **/
void process_debug(struct main_process *pr);

/***
 * [[signal]]
 * Synchronous delivery of signals
 * -------------------------------
 *
 * UNIX signals are delivered to processes in an asynchronous way: when a signal
 * arrives (and it is not blocked), the process is interrupted and the corresponding
 * signal handler function is called. However, most data structures and even most
 * system library calls are not safe with respect to interrupts, so most program
 * using signals contain subtle race conditions and may fail once in a long while.
 *
 * To avoid this problem, the event loop can be asked for synchronous delivery
 * of signals. When a signal registered with @signal_add() arrives, it wakes up
 * the loop (if it is not already awake) and it is processed in the same way
 * as all other events.
 *
 * When used in a multi-threaded program, the signals are delivered to the thread
 * which is currently using the particular main loop context. If the context is not
 * current in any thread, the signals are blocked.
 *
 * As usually with UNIX signals, multiple instances of a single signal can be
 * merged and delivered only once. (Some implementations of the main loop can even
 * drop a signal completely during very intensive signal traffic, when an internal
 * signal queue overflows.)
 ***/

/** Description of a signal to catch. **/
struct main_signal {
  cnode n;
  int signum;					/* [*] Signal to catch */
  void (*handler)(struct main_signal *ms);	/* [*] Called when the signal arrives */
  void *data;					/* [*] For use by the handler */
};

/** Request a signal to be caught and delivered synchronously. **/
void signal_add(struct main_signal *ms);

/** Cancel a request for signal catching. Calling twice is safe. **/
void signal_del(struct main_signal *ms);

/** Tells if a signal catcher is active (i.e., added). **/
static inline int signal_is_active(struct main_signal *ms)
{
  return clist_is_linked(&ms->n);
}

/** Show current state of a signal catcher. Available only if LibUCW has been compiled with `CONFIG_UCW_DEBUG`. **/
void signal_debug(struct main_signal *sg);

#endif
