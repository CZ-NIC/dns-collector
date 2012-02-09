/*
 *	UCW Library -- Main Loop: Record I/O
 *
 *	(c) 2011--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "ucw/mainloop.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

struct rio_buffer {
  cnode n;
  uns full;
  uns written;
  byte buf[];
};

static void
rec_io_timer_expired(struct main_timer *tm)
{
  struct main_rec_io *rio = tm->data;
  timer_del(&rio->timer);
  rio->notify_handler(rio, RIO_ERR_TIMEOUT);
}

static int rec_io_deferred_start_read(struct main_hook *ho);

void
rec_io_add(struct main_rec_io *rio, int fd)
{
  rio->file.fd = fd;
  file_add(&rio->file);
  rio->timer.handler = rec_io_timer_expired;
  rio->timer.data = rio;
  rio->start_read_hook.handler = rec_io_deferred_start_read;
  rio->start_read_hook.data = rio;
  clist_init(&rio->idle_write_buffers);
  clist_init(&rio->busy_write_buffers);
}

void
rec_io_del(struct main_rec_io *rio)
{
  if (!rec_io_is_active(rio))
    return;

  timer_del(&rio->timer);
  hook_del(&rio->start_read_hook);
  file_del(&rio->file);

  if (rio->read_buf)
    {
      DBG("RIO: Freeing read buffer");
      xfree(rio->read_buf);
      rio->read_buf = NULL;
    }

  struct rio_buffer *b;
  while ((b = clist_remove_head(&rio->idle_write_buffers)) || (b = clist_remove_head(&rio->busy_write_buffers)))
    {
      DBG("RIO: Freeing write buffer");
      xfree(b);
    }
}

static int
rec_io_process_read_buf(struct main_rec_io *rio)
{
  uns got;
  while (rio->read_running && (got = rio->read_handler(rio)))
    {
      DBG("RIO READ: Ate %u bytes", got);
      if (got == ~0U)
	return HOOK_IDLE;
      rio->read_rec_start += got;
      rio->read_avail -= got;
      rio->read_prev_avail = 0;
      if (!rio->read_avail)
	{
	  DBG("RIO READ: Resetting buffer");
	  rio->read_rec_start = rio->read_buf;
	  break;
	}
    }
  DBG("RIO READ: Want more");
  return (rio->read_running ? HOOK_RETRY : HOOK_IDLE);
}

static int
rec_io_read_handler(struct main_file *fi)
{
  struct main_rec_io *rio = (struct main_rec_io *) fi;

  if (rio->read_rec_max && rio->read_avail >= rio->read_rec_max)
    {
      rec_io_stop_read(rio);
      rio->notify_handler(rio, RIO_ERR_RECORD_TOO_LARGE);
      return HOOK_IDLE;
    }

restart: ;
  uns rec_start_pos = rio->read_rec_start - rio->read_buf;
  uns rec_end_pos = rec_start_pos + rio->read_avail;
  uns free_space = rio->read_buf_size - rec_end_pos;
  DBG("RIO READ: rec_start=%u avail=%u prev_avail=%u free=%u/%u",
    rec_start_pos, rio->read_avail, rio->read_prev_avail,
    free_space, rio->read_buf_size);
  if (free_space <= rio->read_buf_size/8)
    {
      if (rec_start_pos && rec_start_pos >= rio->read_buf_size/2)
	{
	  // Moving the partial record to the start of the buffer
	  DBG("RIO READ: Moving partial record to start");
	  memmove(rio->read_buf, rio->read_rec_start, rio->read_avail);
	  rio->read_rec_start = rio->read_buf;
	}
      else
	{
	  DBG("RIO READ: Resizing buffer");
	  rio->read_buf_size *= 2;
	  rio->read_buf = xrealloc(rio->read_buf, rio->read_buf_size);
	  rio->read_rec_start = rio->read_buf + rec_start_pos;
	}
      goto restart;
    }

  int l = read(fi->fd, rio->read_buf + rec_end_pos, free_space);
  DBG("RIO READ: Read %d bytes", l);
  if (l < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
	{
	  DBG("RIO READ: Signalling error");
	  rec_io_stop_read(rio);
	  rio->notify_handler(rio, RIO_ERR_READ);
	}
      return HOOK_IDLE;
    }
  if (!l)
    {
      DBG("RIO READ: Signalling EOF");
      rec_io_stop_read(rio);
      rio->notify_handler(rio, RIO_EVENT_EOF);
      return HOOK_IDLE;
    }
  rio->read_prev_avail = rio->read_avail;
  rio->read_avail += l;
  DBG("RIO READ: Available: %u bytes", rio->read_avail);

  return rec_io_process_read_buf(rio);
}

static int
rec_io_deferred_start_read(struct main_hook *ho)
{
  struct main_rec_io *rio = ho->data;

  DBG("RIO: Starting reading");
  if (!rio->read_buf)
    {
      if (!rio->read_buf_size)
	rio->read_buf_size = 256;
      rio->read_buf = xmalloc(rio->read_buf_size);
      DBG("RIO: Created read buffer (%u bytes)", rio->read_buf_size);
      rio->read_rec_start = rio->read_buf;
    }

  rio->file.read_handler = rec_io_read_handler;
  file_chg(&rio->file);
  hook_del(ho);
  rio->read_running = 1;

  rio->read_prev_avail = 0;
  return rec_io_process_read_buf(rio);
}

static void
rec_io_recalc_read(struct main_rec_io *rio)
{
  uns flow = !rio->write_throttle_read || rio->write_watermark < rio->write_throttle_read;
  uns run = rio->read_started && flow;
  DBG("RIO: Recalc read (flow=%u, start=%u) -> %u", flow, rio->read_started, run);
  if (run != rio->read_running)
    {
      if (run)
	{
	  /*
	   * Since we need to rescan the read buffer for leftover records and we
	   * can be deep in the call stack at this moment, we better defer most
	   * of the work to a main_hook, which will be called in the next iteration
	   * of the main loop.
	   */
	  if (!hook_is_active(&rio->start_read_hook))
	    {
	      DBG("RIO: Scheduling start of reading");
	      hook_add(&rio->start_read_hook);
	    }
	}
      else
	{
	  if (hook_is_active(&rio->start_read_hook))
	    {
	      DBG("RIO: Descheduling start of reading");
	      hook_del(&rio->start_read_hook);
	    }
	  rio->file.read_handler = NULL;
	  file_chg(&rio->file);
	  DBG("RIO: Reading stopped");
	  rio->read_running = 0;
	}
    }
}

void
rec_io_start_read(struct main_rec_io *rio)
{
  ASSERT(rec_io_is_active(rio));
  rio->read_started = 1;
  rec_io_recalc_read(rio);
}

void
rec_io_stop_read(struct main_rec_io *rio)
{
  ASSERT(rec_io_is_active(rio));
  rio->read_started = 0;
  rec_io_recalc_read(rio);
}

static void
rec_io_stop_write(struct main_rec_io *rio)
{
  DBG("RIO WRITE: Stopping write");
  // XXX: When we are called after a write error, there might still
  // be some data queued, but we need not care.
  rio->file.write_handler = NULL;
  file_chg(&rio->file);
}

static int
rec_io_write_handler(struct main_file *fi)
{
  struct main_rec_io *rio = (struct main_rec_io *) fi;
  struct rio_buffer *b = clist_head(&rio->busy_write_buffers);
  if (!b)
    {
      rec_io_stop_write(rio);
      return HOOK_IDLE;
    }

  int l = write(fi->fd, b->buf + b->written, b->full - b->written);
  DBG("RIO WRITE: Written %d bytes", l);
  if (l < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
	{
	  rec_io_stop_write(rio);
	  rio->notify_handler(rio, RIO_ERR_WRITE);
	}
      return HOOK_IDLE;
    }
  b->written += l;
  if (b->written == b->full)
    {
      DBG("RIO WRITE: Written full buffer");
      clist_remove(&b->n);
      clist_add_tail(&rio->idle_write_buffers, &b->n);
    }

  rio->write_watermark -= l;
  int ret = HOOK_RETRY;
  if (!rio->write_watermark)
    {
      ret = HOOK_IDLE;
      rec_io_stop_write(rio);
    }
  rec_io_recalc_read(rio);

  // Call the hook, but carefully, because it can delete the RIO structure
  if (rio->notify_handler(rio, rio->write_watermark ? RIO_EVENT_PART_WRITTEN : RIO_EVENT_ALL_WRITTEN) == HOOK_IDLE)
    ret = HOOK_IDLE;
  return ret;
}

static struct rio_buffer *
rec_io_get_buffer(struct main_rec_io *rio)
{
  struct rio_buffer *b = clist_remove_tail(&rio->idle_write_buffers);
  if (b)
    DBG("RIO WRITE: Recycled old buffer");
  else
    {
      if (!rio->write_buf_size)
	rio->write_buf_size = 1024;
      b = xmalloc(sizeof(struct rio_buffer) + rio->write_buf_size);
      DBG("RIO WRITE: Allocated new buffer");
    }
  b->full = b->written = 0;
  return b;
}

void
rec_io_write(struct main_rec_io *rio, void *data, uns len)
{
  byte *bdata = data;
  ASSERT(rec_io_is_active(rio));
  if (!len)
    return;

  while (len)
    {
      struct rio_buffer *b = clist_tail(&rio->busy_write_buffers);
      if (!b || b->full >= rio->write_buf_size)
	{
	  b = rec_io_get_buffer(rio);
	  clist_add_tail(&rio->busy_write_buffers, &b->n);
	}
      uns l = MIN(len, rio->write_buf_size - b->full);
      memcpy(b->buf + b->full, bdata, l);
      b->full += l;
      bdata += l;
      len -= l;
      rio->write_watermark += l;
      DBG("RIO WRITE: Buffered %u bytes of data (total %u)", l, rio->write_watermark);
      rec_io_recalc_read(rio);
    }

  if (!rio->file.write_handler)
    {
      DBG("RIO WRITE: Starting write");
      rio->file.write_handler = rec_io_write_handler;
      file_chg(&rio->file);
    }
}

void
rec_io_set_timeout(struct main_rec_io *rio, timestamp_t expires_delta)
{
  DBG("RIO: Setting timeout %u", (uns) expires_delta);
  if (!expires_delta)
    timer_del(&rio->timer);
  else
    timer_add_rel(&rio->timer, expires_delta);
}

uns
rec_io_parse_line(struct main_rec_io *rio)
{
  for (uns i = rio->read_prev_avail; i < rio->read_avail; i++)
    if (rio->read_rec_start[i] == '\n')
      return i+1;
  return 0;
}

#ifdef TEST

static uns rhand(struct main_rec_io *rio)
{
  uns r = rec_io_parse_line(rio);
  if (r)
    {
      rio->read_rec_start[r-1] = 0;
      printf("Read <%s>\n", rio->read_rec_start);
      if (rio->read_rec_start[0] == '!')
	{
	  rec_io_del(rio);
	  main_shut_down();
	  return ~0U;
	}
      rec_io_set_timeout(rio, 10000);
      rio->read_rec_start[r-1] = '\n';
      rec_io_write(rio, rio->read_rec_start, r);
    }
  return r;
}

static int ehand(struct main_rec_io *rio, int cause)
{
  if (cause < 0 || cause == RIO_EVENT_EOF)
    {
      msg(L_ERROR, "Error %d", cause);
      rec_io_del(rio);
      main_shut_down();
      return HOOK_IDLE;
    }
  else
    {
      msg(L_INFO, "Event %d", cause);
      return HOOK_RETRY;
    }
}

int
main(void)
{
  log_init(NULL);
  main_init();

  struct main_rec_io rio = {};
  rio.read_buf_size = 4;
  rio.read_handler = rhand;
  rio.notify_handler = ehand;
  // rio.read_rec_max = 40;
  rio.write_buf_size = 4;
  rio.write_throttle_read = 6;
  rec_io_add(&rio, 0);
  rec_io_start_read(&rio);
  rec_io_set_timeout(&rio, 10000);

  main_debug();

  main_loop();
  msg(L_INFO, "Finished.");

  if (file_is_active(&rio.file))
    rec_io_del(&rio);
  main_cleanup();
  return 0;
}

#endif
