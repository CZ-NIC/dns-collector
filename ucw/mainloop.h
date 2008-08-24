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

extern timestamp_t main_now;			/* Current time in milliseconds since UNIX epoch */
extern ucw_time_t main_now_seconds;		/* Current time in seconds since the epoch */
extern uns main_shutdown;
extern clist main_timer_list, main_file_list, main_hook_list, main_process_list;

/* User-defined fields are marked with [*], all other fields must be initialized to zero. */

/* Timers */

struct main_timer {
  cnode n;
  timestamp_t expires;
  void (*handler)(struct main_timer *tm); 	/* [*] Function to be called when the timer expires. Must re-add/del the timer.*/
  void *data;					/* [*] Data for use by the handler */
};

void timer_add(struct main_timer *tm, timestamp_t expires);	/* Can modify a running timer, too */
void timer_del(struct main_timer *tm);

void main_get_time(void);			/* Refresh main_now */

/* Files to poll */

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

/* Hooks to be called in each iteration of the main loop */

struct main_hook {
  cnode n;
  int (*handler)(struct main_hook *ho);		/* [*] Hook function; returns HOOK_xxx */
  void *data;					/* [*] For use by the handler */
};

enum main_hook_return {
  HOOK_IDLE,					/* Call again when the main loop becomes idle again */
  HOOK_RETRY,					/* Call again as soon as possible */
  HOOK_DONE = -1,				/* Shut down the main loop if all hooks return this value */
  HOOK_SHUTDOWN = -2				/* Shut down the main loop immediately */
};

void hook_add(struct main_hook *ho);
void hook_del(struct main_hook *ho);

/* Processes to watch */

struct main_process {
  cnode n;
  int pid;					/* Process id (0=not running) */
  int status;					/* Exit status (-1=fork failed) */
  char status_msg[EXIT_STATUS_MSG_SIZE];
  void (*handler)(struct main_process *mp);	/* [*] Called when the process exits; process_del done automatically */
  void *data;					/* [*] For use by the handler */
};

void process_add(struct main_process *mp);
void process_del(struct main_process *mp);
int process_fork(struct main_process *mp);

/* The main loop */

void main_init(void);
void main_loop(void);
void main_debug(void);

#endif
