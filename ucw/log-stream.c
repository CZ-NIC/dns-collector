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
#include "ucw/clists.h"
#include "ucw/simple-lists.h"
#include "ucw/log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <alloca.h>
#include <fcntl.h>

/* Flag indicating initialization of the module */
static int ls_initialized = 0;

/* The head of the list of freed log_streams indexes in ls_streams.ptr (-1 if none free).
 * Freed positions in ls_streams.ptr are connected into a linked list in the following way:
 * ls_streams.ptr[ls_streams_free].idata is the index of next freed position (or -1) */
static int ls_streams_free = -1;

/* Initialize the logstream module.
 * It is not neccessary to call this explicitely as it is called by
 * the first ls_new()  (for backward compatibility and ease of use). */
static void ls_init_module(void)
{
  if (ls_initialized) return;

  /* create the grow array */
  lsbuf_init(&log_streams);
  lsbuf_set_size(&log_streams, LS_INIT_STREAMS);

  bzero(log_streams.ptr, sizeof(struct log_stream*) * (log_streams.len));
  ls_streams_free = -1;

  ls_initialized = 1;

  /* init the default stream (0) as forwarder to fd2 */
  struct log_stream *ls = ls_new();
  ASSERT(ls == log_streams.ptr[0]);
  ASSERT(ls->regnum == 0);
  ls->name = "default";
  ls_add_substream(ls, (struct log_stream *) &ls_default_log);

  /* log this */
  ls_msg(L_DEBUG, "logstream module initialized.");
}

/* close all open streams, un-initialize the module, free all memory,
 * use only ls_default_log */
void ls_close_all(void)
{
  int i;

  if (!ls_initialized) return;

  for (i=0; i<log_streams_after; i++)
  {
    if (log_streams.ptr[i]->regnum>=0)
      ls_close(log_streams.ptr[i]);
    xfree(log_streams.ptr[i]);
  }

  /* set to the default state */
  lsbuf_done(&log_streams);
  log_streams_after=0;
  ls_streams_free=-1;
  ls_initialized = 0;
}

/* add a new substream, malloc()-ate a new simp_node */
void ls_add_substream(struct log_stream *where, struct log_stream *what)
{
  ASSERT(where);
  ASSERT(what);

  simp_node *n = xmalloc(sizeof(simp_node));
  n->p = what;
  clist_add_tail(&(where->substreams), (cnode*)n);
}

/* remove all occurences of a substream, free() the simp_node */
/* return number of deleted entries */
int ls_rm_substream(struct log_stream *where, struct log_stream *what)
{
  void *tmp;
  int cnt=0;
  ASSERT(where);
  ASSERT(what);

  CLIST_FOR_EACH_DELSAFE(simp_node *, i, where->substreams, tmp)
    if (i->p == what)
    {
      clist_remove((cnode*)i);
      xfree(i);
      cnt++;
    }
  return cnt;
}

/* Return a pointer to a new stream with no handler and an empty substream list. */
struct log_stream *ls_new(void)
{
  struct log_stream *l;
  int index;

  /* initialize the array if not initialized already */
  if (unlikely(ls_initialized==0))
    ls_init_module();

  /* there is no closed stream -- allocate a new one */
  if (ls_streams_free==-1)
  {
    /* check the size of the pointer array */
    lsbuf_grow(&log_streams, log_streams_after+1);
    ls_streams_free = log_streams_after++;
    log_streams.ptr[ls_streams_free] = xmalloc(sizeof(struct log_stream));
    log_streams.ptr[ls_streams_free]->idata = -1;
    log_streams.ptr[ls_streams_free]->regnum = -1;
  }

  ASSERT(ls_streams_free>=0);

  /* initialize the stream */
  index = ls_streams_free;
  l = log_streams.ptr[index];
  ls_streams_free = l->idata;
  memset(l, 0, sizeof(struct log_stream));
  l->levels = LS_ALL_LEVELS;
  l->regnum = LS_SET_STRNUM(index);
  l->substreams.head.next = &(l->substreams.head);
  l->substreams.head.prev = &(l->substreams.head);
  return l;
}

/* Close and remember given log_stream */
/* does not affect substreams, but frees the .substreams list */
void ls_close(struct log_stream *ls)
{
  void *tmp;
  ASSERT(ls);

  /* xfree() all the simp_nodes from substreams */
  CLIST_FOR_EACH_DELSAFE(simp_node *, i, ls->substreams, tmp)
  {
    clist_remove((cnode*)i);
    xfree(i);
  }

  /* close and remember the stream */
  if (ls->close!=NULL)
    ls->close(ls);
  ls->idata = ls_streams_free;
  ls_streams_free = LS_GET_STRNUM(ls->regnum);
  ls->regnum = -1;
}
