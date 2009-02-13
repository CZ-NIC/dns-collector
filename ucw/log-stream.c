/*
 *	UCW Library -- Logging: Management of Log Streams
 *
 *	(c) 2008 Tomas Gavenciak <gavento@ucw.cz>
 *	(c) 2009 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/log.h"
#include "ucw/simple-lists.h"

#include <string.h>

/* Initial number of streams to allocate (must be >=2) */
#define LS_INIT_STREAMS 8

/* Flag indicating initialization of the module */
static int log_initialized = 0;

/* The head of the list of freed log_streams indexes in log_streams.ptr (-1 if none free).
 * Freed positions in log_streams.ptr are connected into a linked list in the following way:
 * log_streams.ptr[log_streams_free].idata is the index of next freed position (or -1) */
static int log_streams_free = -1;

/* Initialize the logstream module.
 * It is not neccessary to call this explicitely as it is called by
 * the first log_new_stream()  (for backward compatibility and ease of use). */
static void
log_init_module(void)
{
  if (log_initialized)
    return;

  /* Create the growing array */
  lsbuf_init(&log_streams);
  lsbuf_set_size(&log_streams, LS_INIT_STREAMS);

  bzero(log_streams.ptr, sizeof(struct log_stream*) * (log_streams.len));
  log_streams_free = -1;

  log_initialized = 1;

  /* init the default stream (0) as forwarder to fd2 */
  struct log_stream *ls = log_new_stream();
  ASSERT(ls == log_streams.ptr[0]);
  ASSERT(ls->regnum == 0);
  ls->name = "default";
  log_add_substream(ls, &log_stream_default);
}

/* Close all open streams, un-initialize the module, free all memory,
 * and fall back to using only log_stream_default. */
void
log_close_all(void)
{
  if (!log_initialized)
    return;

  for (int i=0; i < log_streams_after; i++)
    {
      if (log_streams.ptr[i]->regnum >= 0)
	log_close_stream(log_streams.ptr[i]);
      xfree(log_streams.ptr[i]);
    }

  /* Back to the default state */
  lsbuf_done(&log_streams);
  log_streams_after = 0;
  log_streams_free = -1;
  log_initialized = 0;
}

/* Add a new substream. The parent stream takes a reference on the substream,
 * preventing it from being closed as long as it is linked. */
void
log_add_substream(struct log_stream *where, struct log_stream *what)
{
  ASSERT(where);
  ASSERT(what);

  simp_node *n = xmalloc(sizeof(simp_node));
  n->p = log_ref_stream(what);
  clist_add_tail(&where->substreams, &n->n);
}

/* Remove all occurences of a substream together with the references they
 * keep. If a substream becomes unreferenced, it is closed. If what is NULL,
 * all substreams are removed. Returns the number of deleted entries. */
int
log_rm_substream(struct log_stream *where, struct log_stream *what)
{
  void *tmp;
  int cnt = 0;
  ASSERT(where);

  CLIST_FOR_EACH_DELSAFE(simp_node *, i, where->substreams, tmp)
    if (i->p == what || !what)
      {
	clist_remove(&i->n);
	log_close_stream(i->p);
	xfree(i);
	cnt++;
      }
  return cnt;
}

/* Return a pointer to a new stream with no handler and an empty substream list. */
struct log_stream *
log_new_stream(void)
{
  struct log_stream *l;
  int index;

  /* Initialize the data structures if needed */
  log_init_module();

  /* Get a free stream, possibly recycling a closed one */
  if (log_streams_free < 0)
    {
      lsbuf_grow(&log_streams, log_streams_after+1);
      index = log_streams_after++;
      l = log_streams.ptr[index] = xmalloc(sizeof(struct log_stream));
    }
  else
    {
      index = log_streams_free;
      l = log_streams.ptr[index];
      log_streams_free = l->idata;
    }

  /* Initialize the stream */
  bzero(l, sizeof(*l));
  l->levels = LS_ALL_LEVELS;
  l->regnum = LS_SET_STRNUM(index);
  clist_init(&l->substreams);
  return log_ref_stream(l);
}

/* Remove a reference on a stream and close it if it was the last reference.
 * Closing automatically unlinks all substreams and closes them if they are
 * no longer referenced. Returns 1 if the stream has been really closed. */
int
log_close_stream(struct log_stream *ls)
{
  ASSERT(ls);
  ASSERT(ls->use_count);
  if (--ls->use_count)
    return 0;

  /* Unlink all subtreams */
  log_rm_substream(ls, NULL);

  /* Close the stream and add it to the free-list */
  if (ls->close)
    ls->close(ls);
  ls->idata = log_streams_free;
  log_streams_free = LS_GET_STRNUM(ls->regnum);
  ls->regnum = -1;
  return 1;
}
