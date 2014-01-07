/*
 *	UCW Library -- Internals of Logging
 *
 *	(c) 1997--2009 Martin Mares <mj@ucw.cz>
 *	(c) 2008 Tomas Gavenciak <gavento@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_LOG_INTERNAL_H_
#define _UCW_LOG_INTERNAL_H_

#ifdef CONFIG_UCW_CLEAN_ABI
#define log_pass_msg ucw_log_pass_msg
#define log_stream_default ucw_log_stream_default
#define log_streams ucw_log_streams
#define log_streams_after ucw_log_streams_after
#define log_type_names ucw_log_type_names
#endif

/*
 * Pass a message to a stream.
 * @depth prevents loops.
 * Returns 1 in case of loop detection or other fatal error,
 *         0 otherwise
 */
int log_pass_msg(int depth, struct log_stream *ls, struct log_msg *m);

/* Define an array (growing buffer) for pointers to log_streams. */
#define GBUF_TYPE struct log_stream*
#define GBUF_PREFIX(x) lsbuf_##x
#include <ucw/gbuf.h>

extern struct lsbuf_t log_streams;
extern int log_streams_after;

extern struct log_stream log_stream_default;

extern char **log_type_names;

#endif
