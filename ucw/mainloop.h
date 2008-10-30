/*
 *	UCW Library -- Main Loop
 *
 *	(c) 2004--2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_MAINLOOP_H
#define _UCW_MAINLOOP_H

#include "ucw/clists.h"

/***
 * [[conventions]]
 * Conventions
 * -----------
 *
 * The description of structures contain some fields marked as `[*]`.
 * These are the only ones that are user defined. The rest is for
 * internal use and you must initialize it to zeroes.
 ***/

/***
 * [[time]]
 * Time manipulation
 * -----------------
 *
 * This part allows you to know the current time and request
 * to have your function called when the time comes.
 ***/

extern timestamp_t main_now;			/** Current time in milliseconds since UNIX epoch. See @main_get_time(). **/
extern ucw_time_t main_now_seconds;		/** Current time in seconds since the epoch. **/
extern clist main_timer_list, main_file_list, main_hook_list, main_process_list;

/**
 * This is a description of a timer.
 * You fill it with a handler function, any user-defined data and
 * add it using @timer_add().
 *
 * The handler() function must add it again or delete it with
 * @timer_del().
 **/
struct main_timer {
  cnode n;
  timestamp_t expires;
  void (*handler)(struct main_timer *tm);	/* [*] Function to be called when the timer expires. */
  void *data;					/* [*] Data for use by the handler */
};

/**
 * Adds a new timer into the mainloop to be watched and called,
 * when it expires. It can be used to modify an already running
 * timer.
 *
 * The @expire parameter is absolute -- you may use
 * <<var_main_now,`main_now`>>, if you need it relative to now.
 **/
void timer_add(struct main_timer *tm, timestamp_t expires);
/**
 * Removes a timer from the watched ones. You need to call this, when
 * the timer expires and you do not want to use it any more. It can be
 * used to remove a still active timer too.
 **/
void timer_del(struct main_timer *tm);

/**
 * Forces refresh of <<var_main_now,`main_now`>>. You do not usually
 * need to call this, since it is called every time the loop polls for
 * changes. It is here if you need extra precision or some of the
 * hooks takes a long time.
 **/
void main_get_time(void);

/***
 * [[file]]
 * Activity on file descriptors
 * ----------------------------
 *
 * You can let the mainloop watch over a set of file descriptors
 * for changes.
 *
 * //TODO: This probably needs some example how the handlers can be
 * //used, describe the use of this part of module.
 ***/

struct main_file {
  cnode n;
  int fd;					/* [*] File descriptor */
  int (*read_handler)(struct main_file *fi);	/* [*] To be called when ready for reading/writing; must call file_chg() afterwards */
  int (*write_handler)(struct main_file *fi);
  void (*error_handler)(struct main_file *fi, int cause);	/* [*] Handler to call on errors */
  void *data;					/* [*] Data for use by the handlers */
  byte *rbuf;					/* Read/write pointers for use by file_read/write */
  uns rpos, rlen;
  byte *wbuf;
  uns wpos, wlen;
  void (*read_done)(struct main_file *fi); 	/* [*] Called when file_read is finished; rpos < rlen if EOF */
  void (*write_done)(struct main_file *fi);	/* [*] Called when file_write is finished */
  struct main_timer timer;
  struct pollfd *pollfd;
};

enum main_file_err_cause {
  MFERR_READ,
  MFERR_WRITE,
  MFERR_TIMEOUT
};

void file_add(struct main_file *fi);
void file_chg(struct main_file *fi);
void file_del(struct main_file *fi);
void file_read(struct main_file *fi, void *buf, uns len);
void file_write(struct main_file *fi, void *buf, uns len);
void file_set_timeout(struct main_file *fi, timestamp_t expires);
void file_close_all(void);			/* Close all known main_file's; frequently used after fork() */

/***
 * [[hooks]]
 * Loop hooks
 * ----------
 *
 * The hooks can be called whenever the mainloop perform an iteration.
 * You can shutdown the mainloop from within them or request next call
 * only when the loop is idle (for background operations).
 ***/

/**
 * A hook. It contains the function to call and some user data.
 *
 * The handler() must return one value from
 * <<enum_main_hook_return,`main_hook_return`>>.
 **/
struct main_hook {
  cnode n;
  int (*handler)(struct main_hook *ho);		/* [*] Hook function; returns HOOK_xxx */
  void *data;					/* [*] For use by the handler */
};

/**
 * Return value of the hook handler().
 * Specifies what should happen next.
 **/
enum main_hook_return {
  HOOK_IDLE,					/* Call again when the main loop becomes idle again */
  HOOK_RETRY,					/* Call again as soon as possible */
  HOOK_DONE = -1,				/* Shut down the main loop if all hooks return this value */
  HOOK_SHUTDOWN = -2				/* Shut down the main loop immediately */
};

/**
 * Inserts a new hook into the loop.
 **/
void hook_add(struct main_hook *ho);
/**
 * Removes an existing hook from the loop.
 **/
void hook_del(struct main_hook *ho);

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
 * Asks the mainloop to watch this process.
 * As it is done automatically in @process_fork(), you need this only
 * if you removed the process previously by @process_del().
 **/
void process_add(struct main_process *mp);
/**
 * Removes the process from the watched set. This is done
 * automatically, when the process terminates, so you need it only
 * when you do not want to watch a running process any more.
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

/***
 * [[control]]
 * Control of the mainloop
 * -----------------------
 *
 * These functions control the mainloop as a whole.
 ***/

extern uns main_shutdown;			/** Setting this to nonzero forces the @main_loop() function to terminate. **/
void main_init(void);				/** Initializes the mainloop structures. Call before any `*_add` function. **/
/**
 * Start the mainloop.
 * It will watch the provided objects and call callbacks.
 * Terminates when someone sets <<var_main_shutdown,`main_shutdown`>>
 * to nonzero, when all <<hook,hooks>> return
 * <<enum_main_hook_return,`HOOK_DONE`>> or at last one <<hook,hook>>
 * returns <<enum_main_hook_return,`HOOK_SHUTDOWN`>>.
 **/
void main_loop(void);
void main_debug(void);				/** Prints a lot of debug information about current status of the mainloop. **/

#endif
