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

/* user de/allocated program/process name for use in the logsystem */
extern char *ls_title;

struct log_stream
{
  /* optional name, 0-term, de/allocated by constr./destr. or user */
  char *name;
  /* number for use with msg parameter (from LS_SET_STRNUM()), -1 for closed log_stream */
  int regnum;
  /* arbitrary data for filter/handler */
  int idata;
  void *pdata;
  /* severity levels to accept - bitmask of (1<<LEVEL) */
  int levels;
  /* if filter returns nonzero, discard the message */
  int (*filter)(struct log_stream* ls, const char *m, uns cat);
  /* pass the message to these streams (simple-list of pointers) */
  struct clist substreams;
  /* what kind of string to format to pass to the handler (bitmask of LSFMT_xxx ) */
  int msgfmt;
  /* what to do to commit the message (ret 0 on success, nonzero on error)
   * msg is 0-term string, with desired info, one line, ending with "\n\0". */
  int (*handler)(struct log_stream* ls, const char *m, uns cat);
  /* close the log_stream file/connection */
  void (*close)(struct log_stream* ls);
};

/* the default logger */
extern const struct log_stream log_stream_default;

/* A message is processed as follows:
 *  1. Discard if message level not in levels
 *  2. Run filter (if any), discard if ret. nonzero
 *  3. Pass the message to all log_streams in substreams
 *  4. Format the message informaion acc. to msgfmt
 *  5. Run the handler
 */

/* log header verbosity specifying message passed to handler */
enum ls_fmt
{
  LSFMT_LEVEL=1,       /* log severity level (one letter) */
  LSFMT_TIME=2,        /* log time (date-seconds) */
  LSFMT_USEC=4,        /* log also micro-seconds */
  LSFMT_TITLE=8,       /* log program title (global string) */
  LSFMT_PID=16,        /* log program PID */
  LSFMT_LOGNAME=32,    /* log log_stream name */
  LSFMT_NONE=0,
  LSFMT_FULL=LSFMT_LEVEL+LSFMT_TIME+LSFMT_USEC+LSFMT_TITLE+LSFMT_PID+LSFMT_LOGNAME,
  LSFMT_DEFAULT=LSFMT_LEVEL+LSFMT_TIME
};

/* Mask of containing all existing levels. */
#define LS_ALL_LEVELS 0xfff

// return the letter associated with the severity level
#define LS_LEVEL_LETTER(level) ("DIiWwEe!###"[( level )])

///// Macros for extracting parts of the flags parameter
// The division of the flags parameter is decided only here
// The current division is (for 32 bit flags):
// MSB <5 bits: any internal log flags> <8 bits: "user" flags> <10 bits: stream number>
//     <8 bits: severity level> LSB

// Bits per section
enum ls_flagbits {
  LS_LEVEL_BITS    = 8,
  LS_STRNUM_BITS   = 16,
  LS_FLAGS_BITS    = 5,
  LS_INTERNAL_BITS = 4,
};

// Section shifts
enum ls_flagpos {
  LS_LEVEL_POS     = 0,
  LS_STRNUM_POS    = LS_LEVEL_POS + LS_LEVEL_BITS,
  LS_FLAGS_POS     = LS_STRNUM_POS + LS_STRNUM_BITS,
  LS_INTERNAL_POS  = LS_FLAGS_POS + LS_FLAGS_BITS,
};

// Bitmasks
enum ls_flagmasks {
  LS_LEVEL_MASK    = (( 1 << LS_LEVEL_BITS ) - 1 ) << LS_LEVEL_POS,
  LS_STRNUM_MASK   = (( 1 << LS_STRNUM_BITS ) - 1 ) << LS_STRNUM_POS,
  LS_FLAGS_MASK    = (( 1 << LS_FLAGS_BITS ) - 1 ) << LS_FLAGS_POS,
  LS_INTERNAL_MASK = (( 1 << LS_INTERNAL_BITS ) - 1 ) << LS_INTERNAL_POS,
};

// "Get" macros (break flags to parts)
#define LS_GET_LEVEL(flags)     ((( flags ) & LS_LEVEL_MASK ) >> LS_LEVEL_POS )
#define LS_GET_STRNUM(flags)    ((( flags ) & LS_STRNUM_MASK ) >> LS_STRNUM_POS )
#define LS_GET_FLAGS(flags)     ((( flags ) & LS_FLAGS_MASK ) >> LS_FLAGS_POS )
#define LS_GET_INTERNAL(flags)  ((( flags ) & LS_INTERNAL_MASK ) >> LS_INTERNAL_POS )

// "Set" macros (parts to flags)
#define LS_SET_LEVEL(level)     (( level ) << LS_LEVEL_POS )
#define LS_SET_STRNUM(strnum)   (( strnum ) << LS_STRNUM_POS )
#define LS_SET_FLAGS(flags)     (( flags ) << LS_FLAGS_POS )
#define LS_SET_INTERNAL(intern) (( intern ) << LS_INTERNAL_POS )

// Internal flags of the logsystem
// Avoid operations that are unsafe in signal handlers
#define LSFLAG_SIGHANDLER LS_SET_INTERNAL(0x001)

// The module is initialized when a first stream is created.
// Before that only the default stream exists.

/* Return pointer a new (xmalloc()-ated) stream with no handler and an empty substream list. */
struct log_stream *log_new_stream(void);

/* Close and xfree() given log_stream */
/* Does not affect substreams */
void log_close_stream(struct log_stream *ls);

/* close all open streams, un-initialize the module, free all memory,
 * use only ls_default_log */
void log_close_all(void);

/* add a new substream, xmalloc()-ate a new simp_node */
void log_add_substream(struct log_stream *where, struct log_stream *what);

/* remove all occurences of a substream, free() the simp_node */
/* return number of deleted entries */
int log_rm_substream(struct log_stream *where, struct log_stream *what);

/* get a stream by its number (regnum) */
/* returns NULL for free numbers */
/* defaults to ls_default_stream for 0 when stream number 0 not set */
struct log_stream *log_stream_by_flags(uns flags);

/* process a message (string) (INTERNAL) */
/* depth prevents undetected looping */
/* returns 1 in case of loop detection or other fatal error
 *         0 otherwise */
int log_pass_msg(int depth, struct log_stream *ls, const char *stime, const char *sutime,
    const char *msg, uns cat);

/* Define an array (growing buffer) for pointers to log_streams. */
#define GBUF_TYPE struct log_stream*
#define GBUF_PREFIX(x) lsbuf_##x
#include "ucw/gbuf.h"

extern struct lsbuf_t log_streams;
extern int log_streams_after;

/********* Individual handler types (constructors, handlers, destructors) */

/**** standard (filedes) files */

// NOTE:
// under unix, for ordinary files open in append mode, the writes
// are atomic (unless you meet the quota or other bad things happen),
// so using a single log_stream is thread-safe and the file can be shared
// among multiple processes

/* assign log to a file descriptor */
/* initialize with the default formatting, does NOT close the descriptor */
struct log_stream *ls_fdfile_new(int fd);

/* open() a file (append mode) */
/* initialize with the default formatting */
struct log_stream *ls_file_new(const char *path);


/**** syslog */

// NOTE:
// The syslog uses a bit different severity levels, for details, see
// ls_syslog_convert_level().
// syslog also prepends it's own time and severity info, so the default
// messaging passes only clean message

/* assign log to a syslog facility */
/* initialize with no formatting (syslog adds these inforamtion) */
/* name is optional prefix (NULL for none) */
struct log_stream *log_new_syslog(int facility, const char *name);

#endif
