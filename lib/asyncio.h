/*
 *	UCW Library -- Asynchronous I/O
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_ASYNCIO_H
#define _UCW_ASYNCIO_H

#include "lib/semaphore.h"
#include "lib/clists.h"

/* FIXME: Comment, especially on request ordering */

struct asio_queue {
  uns max_requests;			// Maximum number of requests allowed on this queue [user-settable]
  uns buffer_size;			// How large buffers do we use [user-settable]
  uns allocated_requests;
  uns running_requests;
  clist idle_list;			// Recycled requests waiting for get
  clist done_list;			// Requests returned from the worker threads
  sem_t *done_sem;			// ... and how many of them
  clist wait_list;			// Requests available for wait()
};

enum asio_op {
  ASIO_FREE,
  ASIO_READ,
  ASIO_WRITE,
  ASIO_BACKGROUND_WRITE,		// Write with no success notification
};

struct asio_request {
  cnode n;
  struct asio_queue *queue;
  byte *buffer;
  int fd;
  enum asio_op op;
  uns len;
  int status;
};

void asio_init(void);
void asio_cleanup(void);

void asio_init_queue(struct asio_queue *q);			// Initialize a new queue
struct asio_request *asio_get(struct asio_queue *q);		// Get an empty request
void asio_submit(struct asio_request *r);			// Submit the request
struct asio_request *asio_wait(struct asio_queue *q);		// Wait for the first finished request, NULL if no more
void asio_put(struct asio_request *r);				// Return a finished request for recycling
struct asio_request *asio_get_bg(struct asio_queue *q);		// Get and if there are no free requests, wait for background writes to finish
void asio_sync(struct asio_queue *q);				// Wait for all requests to finish

#endif	/* !_UCW_ASYNCIO_H */
