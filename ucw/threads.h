/*
 *	The UCW Library -- Threading Helpers
 *
 *	(c) 2006--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_THREADS_H
#define _UCW_THREADS_H

#include <ucw/sighandler.h>

/* This structure holds per-thread data */

struct ucwlib_context {
  int _thread_id;			// Thread ID (either kernel tid or a counter, use ucwlib_thread_id())
  int temp_counter;			// Counter for fb-temp.c
  struct asio_queue *io_queue;		// Async I/O queue for fb-direct.c
  ucw_sighandler_t *signal_handlers;	// Signal handlers for sighandler.c
  struct main_context *main_context;	// Current context for mainloop.c
  struct cf_context *cf_context;	// Current context for configuration parser
  // Resources and transactions:
  struct respool *current_respool;	// Current resource pool
  struct mempool *trans_pool;		// Transaction mempool
  struct trans *current_trans;		// Currently open transaction
};

#ifdef CONFIG_UCW_THREADS

#ifdef CONFIG_UCW_TLS
extern __thread struct ucwlib_context ucwlib_context;
static inline struct ucwlib_context *ucwlib_thread_context(void) { return &ucwlib_context; }
int ucwlib_thread_id(struct ucwlib_context *c);
#else
struct ucwlib_context *ucwlib_thread_context(void);
static inline int ucwlib_thread_id(struct ucwlib_context *c) { return c->_thread_id; }
#endif

/* Global lock used for initialization, cleanup and other not so frequently accessed global state */

void ucwlib_lock(void);
void ucwlib_unlock(void);

extern uns ucwlib_thread_stack_size;

#else

/* We have no threads, let's simulate the context and locking */

extern struct ucwlib_context default_ucwlib_context;
static inline struct ucwlib_context *ucwlib_thread_context(void) { return &default_ucwlib_context; }

static inline int ucwlib_thread_id(struct ucwlib_context *c UNUSED) { return 0; }

static inline void ucwlib_lock(void) { }
static inline void ucwlib_unlock(void) { }

#endif

#endif
