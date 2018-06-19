/*
 *	UCW Library -- Fast Buffered I/O on itself
 *
 *	(c) 2012 Jan Moskyto Matejka <mq@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/clists.h>
#include <ucw/fastbuf.h>
#include <ucw/mempool.h>

#include <stdio.h>

#define FB_MULTI_NAME "<multi>"

struct fb_multi {
  struct fastbuf fb;
  struct mempool *mp;
  struct subbuf *cur;
  ucw_off_t len;
  clist subbufs;
};

#define FB_MULTI(f) ((struct fb_multi *)(f))

struct subbuf {
  cnode n;
  ucw_off_t begin, end, offset;
  struct fastbuf *fb;
};

static void
fbmulti_get_ptrs(struct fastbuf *f)
{
  struct subbuf *sb = FB_MULTI(f)->cur;
  struct fastbuf *ff = sb->fb;

  f->buffer = ff->buffer;
  f->bptr = ff->bptr;
  f->bstop = ff->bstop;
  f->bufend = ff->bufend;
  f->pos = sb->begin + (ff->pos - sb->offset);
}

static void
fbmulti_set_ptrs(struct fastbuf *f)
{
  FB_MULTI(f)->cur->fb->bptr = f->bptr;
}

static int
fbmulti_subbuf_next(struct fastbuf *f)
{
  struct subbuf *cur = FB_MULTI(f)->cur;
  struct subbuf *next = clist_next(&FB_MULTI(f)->subbufs, &cur->n);
  if (!next)
    return 0;

  // Get the end of current buf if not known yet
  if (!f->seek && !next->begin)
    next->begin = cur->end = f->pos;

  // Set the beginning of the next buf
  if (next->fb->seek)
    {
      bsetpos(next->fb, 0);
      next->offset = 0;
    }
  else
    {
      ASSERT(!f->seek);
      next->offset = btell(next->fb);
    }

  // Set the pointers
  FB_MULTI(f)->cur = next;
  fbmulti_get_ptrs(f);

  return 1;
}

static int
fbmulti_subbuf_prev(struct fastbuf *f)
{
  // Called only when seeking, assuming everything seekable
  struct subbuf *prev = clist_prev(&FB_MULTI(f)->subbufs, &FB_MULTI(f)->cur->n);
  ASSERT(prev);

  // Set pos to beginning, flush offset
  bsetpos(prev->fb, 0);
  prev->offset = 0;

  // Set the pointers
  FB_MULTI(f)->cur = prev;
  fbmulti_get_ptrs(f);

  return 1;
}

static int
fbmulti_refill(struct fastbuf *f)
{
  fbmulti_set_ptrs(f);

  // Refill the subbuf
  uint len = FB_MULTI(f)->cur->fb->refill(FB_MULTI(f)->cur->fb);
  if (len)
    {
      fbmulti_get_ptrs(f);
      return len;
    }

  // Current buf returned EOF
  // Take the next one if exists and redo
  if (fbmulti_subbuf_next(f))
    return fbmulti_refill(f);
  else
    return 0;
}

static int
fbmulti_seek(struct fastbuf *f, ucw_off_t pos, int whence)
{
  fbmulti_set_ptrs(f);
  switch(whence)
    {
    case SEEK_SET:
      if (pos > FB_MULTI(f)->len)
	bthrow(f, "seek", "Seek out of range");

      while (pos > FB_MULTI(f)->cur->end) // Moving forward
	{
	  int r = fbmulti_subbuf_next(f);
	  ASSERT(r);
	}

      while (pos < FB_MULTI(f)->cur->begin) // Moving backwards
	{
	  int r = fbmulti_subbuf_prev(f);
	  ASSERT(r);
	}

      // Now cur is the right buffer.
      FB_MULTI(f)->cur->fb->seek(FB_MULTI(f)->cur->fb, (pos - FB_MULTI(f)->cur->begin), SEEK_SET);

      fbmulti_get_ptrs(f);
      return 1;

    case SEEK_END:
      return fbmulti_seek(f, FB_MULTI(f)->len + pos, SEEK_SET);

    default:
      ASSERT(0);
    }
}

static void
fbmulti_close(struct fastbuf *f)
{
  CLIST_FOR_EACH(struct subbuf *, n, FB_MULTI(f)->subbufs)
    bclose(n->fb);

  mp_delete(FB_MULTI(f)->mp);
}

struct fastbuf *
fbmulti_create(void)
{
  struct mempool *mp = mp_new(1024);
  struct fastbuf *fb_out = mp_alloc_zero(mp, sizeof(struct fb_multi));
  struct fb_multi *fbm = FB_MULTI(fb_out);

  fbm->mp = mp;
  fbm->len = 0;

  clist_init(&fbm->subbufs);

  fb_out->name = FB_MULTI_NAME;
  fb_out->refill = fbmulti_refill;
  fb_out->seek = fbmulti_seek;
  fb_out->close = fbmulti_close;

  return fb_out;
}

void
fbmulti_append(struct fastbuf *f, struct fastbuf *fb)
{
  struct subbuf *last = clist_tail(&FB_MULTI(f)->subbufs);

  struct subbuf *sb = mp_alloc(FB_MULTI(f)->mp, sizeof(*sb));
  sb->fb = fb;
  clist_add_tail(&FB_MULTI(f)->subbufs, &(sb->n));

  ASSERT(fb->refill);

  if (fb->seek)
    {
      if (f->seek)
	{
	  sb->begin = last ? last->end : 0;
	  bseek(fb, 0, SEEK_END);
	  FB_MULTI(f)->len = sb->end = sb->begin + btell(fb);
	}

      bsetpos(fb, 0);
    }

  else
    {
      f->seek = NULL;
      sb->offset = btell(fb);
      sb->begin = 0;
      sb->end = 0;
    }

  if (!last)
    {
      FB_MULTI(f)->cur = sb;
      fbmulti_get_ptrs(f);
    }
}

void
fbmulti_remove(struct fastbuf *f, struct fastbuf *fb)
{
  bflush(f);
  if (fb)
    {
      CLIST_FOR_EACH(struct subbuf *, n, FB_MULTI(f)->subbufs)
	if (fb == n->fb)
	  {
	    clist_remove(&(n->n));
	    return;
	  }

      die("Given fastbuf %p not in given fbmulti %p", fb, f);
    }
  else
    clist_init(&FB_MULTI(f)->subbufs);
}

#ifdef TEST

int main(int argc, char **argv)
{
  if (argc < 2)
    {
      fprintf(stderr, "You must specify a test (r, m, i, n)\n");
      return 1;
    }
  switch (*argv[1])
    {
      case 'r':
        {
	  char *data[] = { "One\nLine", "Two\nLines", "Th\nreeLi\nnes\n" };
	  struct fastbuf fb[ARRAY_SIZE(data)];
	  for (uint i=0;i<ARRAY_SIZE(data);i++)
	    fbbuf_init_read(&fb[i], data[i], strlen(data[i]), 0);

	  struct fastbuf *f = fbmulti_create();
	  fbmulti_append(f, &fb[0]);
	  fbmulti_append(f, &fb[1]);
	  fbmulti_append(f, &fb[2]);

	  char buffer[9];
	  while (bgets(f, buffer, 9))
	    puts(buffer);

	  bclose(f);
	  break;
        }
      case 'm':
	{
	  char *data[] = { "Mnl", "ige" };
	  struct fastbuf fb[ARRAY_SIZE(data)];
	  for (uint i=0;i<ARRAY_SIZE(data);i++)
	    fbbuf_init_read(&fb[i], data[i], strlen(data[i]), 0);

	  struct fastbuf *f = fbmulti_create();
	  fbmulti_append(f, &fb[0]);
	  fbmulti_append(f, &fb[1]);

	  int pos[] = {0, 3, 1, 4, 2, 5};

	  for (uint i=0;i<ARRAY_SIZE(pos);i++)
	    {
	      bsetpos(f, pos[i]);
	      putchar(bgetc(f));
	    }

	  bclose(f);
	  break;
	}
      case 'i':
	{
	  char *data = "Insae";
	  struct fastbuf fb[4];
	  fbbuf_init_read(&fb[0], data, 1, 0);
	  fbbuf_init_read(&fb[1], data + 1, 1, 0);
	  fbbuf_init_read(&fb[2], data + 2, 2, 0);
	  fbbuf_init_read(&fb[3], data + 4, 1, 0);

	  struct fastbuf *f = fbmulti_create();
	  fbmulti_append(f, &fb[0]);
	  fbmulti_append(f, &fb[1]);
	  fbmulti_append(f, &fb[2]);
	  fbmulti_append(f, &fb[1]);
	  fbmulti_append(f, &fb[3]);

	  char buffer[9];
	  while(bgets(f, buffer, 9))
	    puts(buffer);

	  bclose(f);
	  break;
	}
      case 'n':
	{
	  char *data[] = { "Nested", "Data", "As", "In", "Real", "Usage", };
	  struct fastbuf fb[ARRAY_SIZE(data)];
	  for (uint i=0;i<ARRAY_SIZE(data);i++)
	    fbbuf_init_read(&fb[i], data[i], strlen(data[i]), 0);

	  struct fastbuf sp;
	  fbbuf_init_read(&sp, " ", 1, 0);

	  struct fastbuf nl;
	  fbbuf_init_read(&nl, "\n", 1, 0);

	  struct fastbuf *f = fbmulti_create();
	  struct fastbuf *ff;

	  ff = fbmulti_create();
	  fbmulti_append(ff, &fb[0]);
	  fbmulti_append(ff, &sp);
	  fbmulti_append(ff, &fb[1]);
	  fbmulti_append(f, ff);
	  fbmulti_append(f, &nl);

	  ff = fbmulti_create();
	  fbmulti_append(ff, &fb[2]);
	  fbmulti_append(ff, &sp);
	  fbmulti_append(ff, &fb[3]);
	  fbmulti_append(f, ff);
	  fbmulti_append(f, &nl);

	  ff = fbmulti_create();
	  fbmulti_append(ff, &fb[4]);
	  fbmulti_append(ff, &sp);
	  fbmulti_append(ff, &fb[5]);
	  fbmulti_append(f, ff);
	  fbmulti_append(f, &nl);

	  char buffer[20];
	  while (bgets(f, buffer, 20))
	    puts(buffer);

	  bclose(f);
	  break;
	}
    }
  return 0;
}

#endif
