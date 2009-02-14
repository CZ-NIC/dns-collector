/*
 *	UCW Library -- Logging
 *
 *	(c) 1997--2009 Martin Mares <mj@ucw.cz>
 *	(c) 2008 Tomas Gavenciak <gavento@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_LOG_H_
#define _UCW_LOG_H_

#include "ucw/clists.h"

/*** === Messages and streams ***/

/**
 * Inside the logging system, a log message is always represented by this structure.
 **/
struct log_msg {
  char *m;				// The formatted message itself, ending with \n\0
  int m_len;				// Length without the \0
  struct tm *tm;			// Current time
  uns flags;				// Category and other flags as passed to msg()
  char *raw_msg;			// Unformatted parts
  char *stime;
  char *sutime;
};

/**
 * Each stream is represented by an instance of this structure.
 **/
struct log_stream {
  char *name;				// Optional name, allocated by the user (or constructor)
  int regnum;				// Stream number, already encoded by LS_SET_STRNUM(); -1 if closed
  uns levels;				// Bitmask of accepted severity levels (default: all)
  uns msgfmt;				// Formatting flags (LSFMT_xxx)
  uns use_count;			// Number of references to the stream
  int (*filter)(struct log_stream* ls, struct log_msg *m);	// Filter function, return non-zero to discard the message
  clist substreams;			// Pass the message to these streams (simple_list of pointers)
  int (*handler)(struct log_stream *ls, struct log_msg *m);	// Called to commit the message
  void (*close)(struct log_stream* ls);	// Called upon log_close_stream()
  // Private data of the handler follow
};

/**
 * Formatting flags specifying the format of the message passed to the handler.
 **/
enum ls_fmt {
  LSFMT_LEVEL =		1,		// severity level (one letter) */
  LSFMT_TIME =		2,		// date and time (YYYY-mm-dd HH:MM:SS) */
  LSFMT_USEC = 		4,		// also micro-seconds */
  LSFMT_TITLE =		8,		// program title (log_title) */
  LSFMT_PID =		16,		// program PID (log_pid) */
  LSFMT_LOGNAME =	32,		// name of the log_stream */
};

#define LSFMT_DEFAULT (LSFMT_LEVEL | LSFMT_TIME)	/** Default format **/

// Return the letter associated with a given severity level
#define LS_LEVEL_LETTER(level) ("DIiWwEe!###"[( level )])

/***
 * === Message flags
 *
 * The @flags parameter of msg() is divided to several groups of bits (from the LSB):
 * message severity level (`L_xxx`), destination stream, message type [currently unused]
 * and control bits (e.g., `L_SIGHANDLER`).
 ***/

enum ls_flagbits {			// Bit widths of groups
  LS_LEVEL_BITS =	8,
  LS_STRNUM_BITS =	16,
  LS_TYPE_BITS =	5,
  LS_CTRL_BITS =	3,
};

enum ls_flagpos {			// Bit positions of groups
  LS_LEVEL_POS =	0,
  LS_STRNUM_POS =	LS_LEVEL_POS + LS_LEVEL_BITS,
  LS_TYPE_POS =		LS_STRNUM_POS + LS_STRNUM_BITS,
  LS_CTRL_POS =		LS_TYPE_POS + LS_TYPE_BITS,
};

enum ls_flagmasks {			// Bit masks of groups
  LS_LEVEL_MASK =	((1 << LS_LEVEL_BITS) - 1) << LS_LEVEL_POS,
  LS_STRNUM_MASK =	((1 << LS_STRNUM_BITS) - 1) << LS_STRNUM_POS,
  LS_TYPE_MASK =	((1 << LS_TYPE_BITS) - 1) << LS_TYPE_POS,
  LS_CTRL_MASK =	((1 << LS_CTRL_BITS) - 1) << LS_CTRL_POS,
};

// "Get" macros (break flags to parts)
#define LS_GET_LEVEL(flags)	(((flags) & LS_LEVEL_MASK) >> LS_LEVEL_POS)	/** Extract severity level **/
#define LS_GET_STRNUM(flags)	(((flags) & LS_STRNUM_MASK) >> LS_STRNUM_POS)	/** Extract stream number **/
#define LS_GET_TYPE(flags)	(((flags) & LS_TYPE_MASK) >> LS_TYPE_POS)	/** Extract message type **/
#define LS_GET_CTRL(flags)	(((flags) & LS_CTRL_MASK) >> LS_CTRL_POS)	/** Extract control bits **/

// "Set" macros (parts to flags)
#define LS_SET_LEVEL(level)	((level) << LS_LEVEL_POS)			/** Convert severity level to flags **/
#define LS_SET_STRNUM(strnum)	((strnum) << LS_STRNUM_POS)			/** Convert stream number to flags **/
#define LS_SET_TYPE(type)	((type) << LS_TYPE_POS)				/** Convert message type to flags **/
#define LS_SET_CTRL(ctrl)	((ctrl) << LS_CTRL_POS)				/** Convert control bits to flags **/

/*** === Operations on streams ***/

/**
 * Allocate a new log stream with no handler and an empty substream list.
 * Since struct log_stream is followed by private data, @size bytes of memory are allocated
 * for the whole structure. See below for functions creating specific stream types.
 **/
struct log_stream *log_new_stream(size_t size);

/**
 * Decrement the use count of a stream. If it becomes zero, close the stream,
 * free its memory, and unlink all its substreams.
 **/
int log_close_stream(struct log_stream *ls);

/**
 * Get a new reference on an existing stream. For convenience, the return value is
 * equal to the argument @ls.
 **/
static inline struct log_stream *log_ref_stream(struct log_stream *ls)
{
  ls->use_count++;
  return ls;
}

/**
 * Link a substream to a stream. The substream gains a reference.
 **/
void log_add_substream(struct log_stream *where, struct log_stream *what);

/**
 * Unlink all occurrences of a substream @what from stream @where. Each
 * occurrence loses a reference. If @what is NULL, all substreams are unlinked.
 * Returns the number of unlinked substreams.
 **/
int log_rm_substream(struct log_stream *where, struct log_stream *what);

/**
 * Set formatting flags of a given stream and all its substreams. The flags are
 * AND'ed with @mask and OR'ed with @data.
 **/
void log_set_format(struct log_stream *ls, uns mask, uns data);

/**
 * Find a stream by its registration number (in the format of logging flags).
 * Returns NULL if there is no such stream.
 **/
struct log_stream *log_stream_by_flags(uns flags);

/**
 * Return a pointer to the default stream (stream #0).
 **/
static inline struct log_stream *log_default_stream(void)
{
  return log_stream_by_flags(0);
}

/**
 * Close all open streams, un-initialize the module, free all memory and
 * reset the logging mechanism to use stderr only.
 **/
void log_close_all(void);

/***
 * === Logging to files
 *
 * All log files are open in append mode, which guarantees atomicity of write()
 * even in multi-threaded programs.
 ***/

struct log_stream *log_new_file(const char *path);		/** Create a stream bound to a log file. **/
struct log_stream *log_new_fd(int fd);				/** Create a stream bound to a file descriptor. **/

/**
 * When a time-based name of the log file changes, the logger switches to a new
 * log file automatically. This can be sometimes inconvenient, so you can use
 * this function to disable the automatic switches. The calls to this function
 * can be nested.
 **/
void log_switch_disable(void);
void log_switch_enable(void);		/** Negate the effect of log_switch_disable(). **/
int log_switch(void);			/** Switch log files manually. **/

/***
 * === Logging to syslog
 *
 * This log stream uses the libc interface to the system logging daemon (`syslogd`).
 * As syslog serverities differ from our scheme, they are translated; if you
 * are interested in details, search for syslog_level().
 *
 * Syslog also provides its own timestamps, so we turn off all formatting
 * of the LibUCW logger.
 ***/

/**
 * Create a log stream for logging to a selected syslog facility.
 * The @name is an optional prefix of the messages.
 **/
struct log_stream *log_new_syslog(int facility, const char *name);

#endif
