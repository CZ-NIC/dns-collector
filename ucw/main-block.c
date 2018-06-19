/*
 *	UCW Library -- Main Loop: Block I/O
 *
 *	(c) 2004--2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/mainloop.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static void
block_io_timer_expired(struct main_timer *tm)
{
  struct main_block_io *bio = tm->data;
  timer_del(&bio->timer);
  if (bio->error_handler)
    bio->error_handler(bio, BIO_ERR_TIMEOUT);
}

void
block_io_add(struct main_block_io *bio, int fd)
{
  bio->file.fd = fd;
  file_add(&bio->file);
  bio->timer.handler = block_io_timer_expired;
  bio->timer.data = bio;
}

void
block_io_del(struct main_block_io *bio)
{
  timer_del(&bio->timer);
  file_del(&bio->file);
}

static int
block_io_read_handler(struct main_file *fi)
{
  struct main_block_io *bio = (struct main_block_io *) fi;

  while (bio->rpos < bio->rlen)
    {
      int l = read(fi->fd, bio->rbuf + bio->rpos, bio->rlen - bio->rpos);
      DBG("BIO: FD %d: read %d", fi->fd, l);
      if (l < 0)
	{
	  if (errno != EINTR && errno != EAGAIN && bio->error_handler)
	    bio->error_handler(bio, BIO_ERR_READ);
	  return HOOK_IDLE;
	}
      else if (!l)
	break;
      bio->rpos += l;
    }
  DBG("BIO: FD %d done read %d of %d", fi->fd, bio->rpos, bio->rlen);
  fi->read_handler = NULL;
  file_chg(fi);
  bio->read_done(bio);
  return HOOK_RETRY;
}

static int
block_io_write_handler(struct main_file *fi)
{
  struct main_block_io *bio = (struct main_block_io *) fi;

  while (bio->wpos < bio->wlen)
    {
      int l = write(fi->fd, bio->wbuf + bio->wpos, bio->wlen - bio->wpos);
      DBG("BIO: FD %d: write %d", fi->fd, l);
      if (l < 0)
	{
	  if (errno != EINTR && errno != EAGAIN && bio->error_handler)
	    bio->error_handler(bio, BIO_ERR_WRITE);
	  return HOOK_IDLE;
	}
      bio->wpos += l;
    }
  DBG("BIO: FD %d done write %d", fi->fd, bio->wpos);
  fi->write_handler = NULL;
  file_chg(fi);
  bio->write_done(bio);
  return HOOK_RETRY;
}

void
block_io_read(struct main_block_io *bio, void *buf, uint len)
{
  ASSERT(bio->file.n.next);
  if (len)
    {
      bio->file.read_handler = block_io_read_handler;
      bio->rbuf = buf;
      bio->rpos = 0;
      bio->rlen = len;
    }
  else
    {
      bio->file.read_handler = NULL;
      bio->rbuf = NULL;
      bio->rpos = bio->rlen = 0;
    }
  file_chg(&bio->file);
}

void
block_io_write(struct main_block_io *bio, const void *buf, uint len)
{
  ASSERT(bio->file.n.next);
  if (len)
    {
      bio->file.write_handler = block_io_write_handler;
      bio->wbuf = buf;
      bio->wpos = 0;
      bio->wlen = len;
    }
  else
    {
      bio->file.write_handler = NULL;
      bio->wbuf = NULL;
      bio->wpos = bio->wlen = 0;
    }
  file_chg(&bio->file);
}

void
block_io_set_timeout(struct main_block_io *bio, timestamp_t expires_delta)
{
  if (!expires_delta)
    timer_del(&bio->timer);
  else
    timer_add_rel(&bio->timer, expires_delta);
}
