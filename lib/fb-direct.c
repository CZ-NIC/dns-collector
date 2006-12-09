/*
 *	UCW Library -- Fast Buffered I/O on O_DIRECT Files
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *	This is a fastbuf backend for fast streaming I/O using O_DIRECT and
 *	the asynchronous I/O module. It's designed for use on large files
 *	which don't fit in the disk cache.
 *
 *	CAVEATS:
 *
 *	  - All operations with a single fbdirect handle must be done
 *	    within a single thread, unless you provide a custom I/O queue
 *	    and take care of locking.
 *
 *	FIXME: what if the OS doesn't support O_DIRECT?
 *	FIXME: doc: don't mix threads
 *	FIXME: unaligned seeks and partial writes?
 *	FIXME: merge with other file-oriented fastbufs
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"
#include "lib/asio.h"
#include "lib/conf.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

static uns fbdir_cheat;
static uns fbdir_buffer_size = 65536;
static uns fbdir_read_ahead = 1;
static uns fbdir_write_back = 1;

static struct cf_section fbdir_cf = {
  CF_ITEMS {
    CF_UNS("Cheat", &fbdir_cheat),
    CF_UNS("BufferSize", &fbdir_buffer_size),
    CF_UNS("ReadAhead", &fbdir_read_ahead),
    CF_UNS("WriteBack", &fbdir_write_back),
    CF_END
  }
};

#define FBDIR_ALIGN 512

static pthread_key_t fbdir_queue_key;

enum fbdir_mode {				// Current operating mode
    M_NULL,
    M_READ,
    M_WRITE
};

struct fb_direct {
  struct fastbuf fb;
  int fd;					// File descriptor
  int is_temp_file;				// 0=normal file, 1=temporary file, delete on close, -1=shared FD
  struct asio_queue *io_queue;			// I/O queue to use
  struct asio_queue *user_queue;		// If io_queue was supplied by the user
  struct asio_request *pending_read;
  struct asio_request *done_read;
  struct asio_request *active_buffer;
  enum fbdir_mode mode;
  byte name[0];
};
#define FB_DIRECT(f) ((struct fb_direct *)(f)->is_fastbuf)

static void CONSTRUCTOR
fbdir_global_init(void)
{
  cf_declare_section("FBDirect", &fbdir_cf, 0);
  if (pthread_key_create(&fbdir_queue_key, NULL) < 0)
    die("Cannot create fbdir_queue_key: %m");
}

static void
fbdir_read_sync(struct fb_direct *F)
{
  while (F->pending_read)
    {
      struct asio_request *r = asio_wait(F->io_queue);
      ASSERT(r);
      struct fb_direct *G = r->user_data;
      ASSERT(G);
      ASSERT(G->pending_read == r && !G->done_read);
      G->pending_read = NULL;
      G->done_read = r;
    }
}

static void
fbdir_change_mode(struct fb_direct *F, enum fbdir_mode mode)
{
  if (F->mode == mode)
    return;
  DBG("FB-DIRECT: Switching mode to %d", mode);
  switch (F->mode)
    {
    case M_NULL:
      break;
    case M_READ:
      fbdir_read_sync(F);			// Wait for read-ahead requests to finish
      if (F->done_read)				// Return read-ahead requests if any
	{
	  asio_put(F->done_read);
	  F->done_read = NULL;
	}
      break;
    case M_WRITE:
      asio_sync(F->io_queue);			// Wait for pending writebacks
      break;
    }
  if (F->active_buffer)
    {
      asio_put(F->active_buffer);
      F->active_buffer = NULL;
    }
  F->mode = mode;
}

static void
fbdir_submit_read(struct fb_direct *F)
{
  struct asio_request *r = asio_get(F->io_queue);
  r->fd = F->fd;
  r->op = ASIO_READ;
  r->len = F->io_queue->buffer_size;
  r->user_data = F;
  asio_submit(r);
  F->pending_read = r;
}

static int
fbdir_refill(struct fastbuf *f)
{
  struct fb_direct *F = FB_DIRECT(f);

  DBG("FB-DIRECT: Refill");

  if (!F->done_read)
    {
      if (!F->pending_read)
	{
	  fbdir_change_mode(F, M_READ);
	  fbdir_submit_read(F);
	}
      fbdir_read_sync(F);
      ASSERT(F->done_read);
    }

  struct asio_request *r = F->done_read;
  F->done_read = NULL;
  if (F->active_buffer)
    asio_put(F->active_buffer);
  F->active_buffer = r;
  if (!r->status)
    return 0;
  if (r->status < 0)
    die("Error reading %s: %s", f->name, strerror(r->returned_errno));
  f->bptr = f->buffer = r->buffer;
  f->bstop = f->bufend = f->buffer + r->status;
  f->pos += r->status;

  fbdir_submit_read(F);				// Read-ahead the next block

  return r->status;
}

static void
fbdir_spout(struct fastbuf *f)
{
  struct fb_direct *F = FB_DIRECT(f);
  struct asio_request *r;

  DBG("FB-DIRECT: Spout");

  fbdir_change_mode(F, M_WRITE);
  r = F->active_buffer;
  if (r && f->bptr > f->bstop)
    {
      r->op = ASIO_WRITE_BACK;
      r->fd = F->fd;
      r->len = f->bptr - f->bstop;
      ASSERT(!(f->pos % FBDIR_ALIGN) || fbdir_cheat);
      f->pos += r->len;
      if (!fbdir_cheat && r->len % FBDIR_ALIGN)			// Have to simulate incomplete writes
	{
	  r->len = ALIGN_TO(r->len, FBDIR_ALIGN);
	  asio_submit(r);
	  asio_sync(F->io_queue);
	  DBG("FB-DIRECT: Truncating at %Ld", (long long)f->pos);
	  if (sh_ftruncate(F->fd, f->pos) < 0)
	    die("Error truncating %s: %m", f->name);
	}
      else
	asio_submit(r);
      r = NULL;
    }
  if (!r)
    r = asio_get(F->io_queue);
  f->bstop = f->bptr = f->buffer = r->buffer;
  f->bufend = f->buffer + F->io_queue->buffer_size;
  F->active_buffer = r;
}

static void
fbdir_seek(struct fastbuf *f, sh_off_t pos, int whence)
{
  DBG("FB-DIRECT: Seek %Ld %d", (long long)pos, whence);

  if (whence == SEEK_SET && pos == f->pos)
    return;

  fbdir_change_mode(FB_DIRECT(f), M_NULL);			// Wait for all async requests to finish
  sh_off_t l = sh_seek(FB_DIRECT(f)->fd, pos, whence);
  if (l < 0)
    die("lseek on %s: %m", f->name);
  f->pos = l;
}

static struct asio_queue *
fbdir_get_io_queue(void)
{
  struct asio_queue *q = pthread_getspecific(fbdir_queue_key);
  if (!q)
    {
      q = xmalloc_zero(sizeof(struct asio_queue));
      q->buffer_size = fbdir_buffer_size;
      q->max_writebacks = fbdir_write_back;
      asio_init_queue(q);
      pthread_setspecific(fbdir_queue_key, q);
    }
  q->use_count++;
  DBG("FB-DIRECT: Got I/O queue, uc=%d", q->use_count);
  return q;
}

static void
fbdir_put_io_queue(void)
{
  struct asio_queue *q = pthread_getspecific(fbdir_queue_key);
  ASSERT(q);
  DBG("FB-DIRECT: Put I/O queue, uc=%d", q->use_count);
  if (!--q->use_count)
    {
      asio_cleanup_queue(q);
      xfree(q);
      pthread_setspecific(fbdir_queue_key, NULL);
    }
}

static void
fbdir_close(struct fastbuf *f)
{
  struct fb_direct *F = FB_DIRECT(f);

  DBG("FB-DIRECT: Close");

  fbdir_change_mode(F, M_NULL);
  if (!F->user_queue)
    fbdir_put_io_queue();

  switch (F->is_temp_file)
    {
    case 1:
      if (unlink(f->name) < 0)
	log(L_ERROR, "unlink(%s): %m", f->name);
    case 0:
      close(F->fd);
    }

  xfree(f);
}

static int
fbdir_config(struct fastbuf *f, uns item, int value)
{
  switch (item)
    {
    case BCONFIG_IS_TEMP_FILE:
      FB_DIRECT(f)->is_temp_file = value;
      return 0;
    default:
      return -1;
    }
}

static struct fastbuf *
fbdir_open_internal(byte *name, int fd, struct asio_queue *q)
{
  int namelen = strlen(name) + 1;
  struct fb_direct *F = xmalloc(sizeof(struct fb_direct) + namelen);
  struct fastbuf *f = &F->fb;

  DBG("FB-DIRECT: Open");
  bzero(F, sizeof(*F));
  f->name = F->name;
  memcpy(f->name, name, namelen);
  F->fd = fd;
  if (q)
    F->io_queue = F->user_queue = q;
  else
    F->io_queue = fbdir_get_io_queue();
  f->refill = fbdir_refill;
  f->spout = fbdir_spout;
  f->seek = fbdir_seek;
  f->close = fbdir_close;
  f->config = fbdir_config;
  f->can_overwrite_buffer = 2;
  return f;
}

struct fastbuf *
fbdir_open_try(byte *name, uns mode, struct asio_queue *q)
{
  if (!fbdir_cheat)
    mode |= O_DIRECT;
  int fd = sh_open(name, mode, 0666);
  if (fd < 0)
    return NULL;
  struct fastbuf *b = fbdir_open_internal(name, fd, q);
  if (mode & O_APPEND)
    fbdir_seek(b, 0, SEEK_END);
  return b;
}

struct fastbuf *
fbdir_open(byte *name, uns mode, struct asio_queue *q)
{
  struct fastbuf *b = fbdir_open_try(name, mode, q);
  if (!b)
    die("Unable to %s file %s: %m",
	(mode & O_CREAT) ? "create" : "open", name);
  return b;
}

struct fastbuf *
fbdir_open_fd(int fd, struct asio_queue *q)
{
  byte x[32];

  sprintf(x, "fd%d", fd);
  if (!fbdir_cheat && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_DIRECT) < 0)
    log(L_WARN, "Cannot set O_DIRECT on fd %d: %m", fd);
  return fbdir_open_internal(x, fd, q);
}

#ifdef TEST

#include "lib/getopt.h"

int main(int argc, char **argv)
{
  struct fastbuf *f, *t;

  log_init(NULL);
  if (cf_getopt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) >= 0)
    die("Hey, whaddya want?");
  f = (optind < argc) ? fbdir_open(argv[optind++], O_RDONLY, NULL) : fbdir_open_fd(0, NULL);
  t = (optind < argc) ? fbdir_open(argv[optind++], O_RDWR | O_CREAT | O_TRUNC, NULL) : fbdir_open_fd(1, NULL);

  bbcopy(f, t, ~0U);
  ASSERT(btell(f) == btell(t));

#if 0		// This triggers unaligned write
  bflush(t);
  bputc(t, '\n');
#endif

  brewind(t);
  bgetc(t);
  ASSERT(btell(t) == 1);

  bclose(f);
  bclose(t);
  return 0;
}

#endif
