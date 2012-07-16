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
  clist *subbufs;
};

#define FB_MULTI(f) ((struct fb_multi *)(f))

struct subbuf {
  cnode n;
  ucw_off_t begin, end, offset;
  int allow_close;
  struct fastbuf *fb;
};

static void
fbmulti_subbuf_get_end(struct subbuf *s)
{
  ASSERT(s->fb->seek);
  bseek(s->fb, 0, SEEK_END);
  s->end = s->begin + btell(s->fb);
}

static void
fbmulti_get_ptrs(struct fastbuf *f)
{
  f->buffer = FB_MULTI(f)->cur->fb->buffer;
  f->bptr = FB_MULTI(f)->cur->fb->bptr;
  f->bstop = FB_MULTI(f)->cur->fb->bstop;
  f->bufend = FB_MULTI(f)->cur->fb->bufend;
  f->pos = FB_MULTI(f)->cur->begin + FB_MULTI(f)->cur->fb->pos - FB_MULTI(f)->cur->offset;
}

static void
fbmulti_set_ptrs(struct fastbuf *f)
{
  FB_MULTI(f)->cur->fb->bptr = f->bptr;
}

static int
fbmulti_subbuf_next(struct fastbuf *f)
{
  struct subbuf *next = clist_next(FB_MULTI(f)->subbufs, &FB_MULTI(f)->cur->n);
  if (next == NULL)
    return 0;

  // Check the end of current buf
  if (f->seek)
    {
      FB_MULTI(f)->cur->fb->seek(FB_MULTI(f)->cur->fb, FB_MULTI(f)->cur->end - FB_MULTI(f)->cur->begin, SEEK_SET);
      fbmulti_get_ptrs(f);
      ASSERT(FB_MULTI(f)->cur->end == f->pos);
    }
  else
    FB_MULTI(f)->cur->end = f->pos;

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

  next->begin = FB_MULTI(f)->cur->end;

  // Set the pointers
  FB_MULTI(f)->cur = next;
  fbmulti_get_ptrs(f);

  return 1;
}

static int
fbmulti_subbuf_prev(struct fastbuf *f)
{
  // Called only when seeking, assuming everything seekable
  struct subbuf *prev = clist_prev(FB_MULTI(f)->subbufs, &FB_MULTI(f)->cur->n);
  ASSERT(prev != NULL);

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
  uns len = FB_MULTI(f)->cur->fb->refill(FB_MULTI(f)->cur->fb);
  if (len)
    {
      fbmulti_get_ptrs(f);
      return len;
    }

  // Current buf returned EOF
  // Update the information on end of this buffer
  fbmulti_subbuf_get_end(FB_MULTI(f)->cur);

  // Take the next one if exists and redo
  if (fbmulti_subbuf_next(f))
    return fbmulti_refill(f);
  else
    return 0;
}

static void
fbmulti_get_len(struct fastbuf *f)
{
  ucw_off_t pos = btell(f);
  ASSERT(f->seek);
  FB_MULTI(f)->len = 0;

  CLIST_FOR_EACH(struct subbuf *, n, *(FB_MULTI(f)->subbufs))
    {
      n->begin = FB_MULTI(f)->len;
      fbmulti_subbuf_get_end(n);
      FB_MULTI(f)->len = n->end;
    }
  f->seek(f, pos, SEEK_SET); // XXX: f->seek is needed here instead of bsetpos as the FE assumptions about f's state may be completely wrong.
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
	ASSERT(fbmulti_subbuf_next(f));

      while (pos < FB_MULTI(f)->cur->begin) // Moving backwards
	ASSERT(fbmulti_subbuf_prev(f));

      // Now cur is the right buffer.
      FB_MULTI(f)->cur->fb->seek(FB_MULTI(f)->cur->fb, (pos - FB_MULTI(f)->cur->begin), SEEK_SET);

      fbmulti_get_ptrs(f);
      return 1;
      break;

    case SEEK_END:
      return fbmulti_seek(f, FB_MULTI(f)->len+pos, SEEK_SET);
      break;

    default:
      ASSERT(0);
    }
}

static void
fbmulti_update_capability(struct fastbuf *f)
{
  // FB Multi is only a proxy to other fastbufs ... if any of them lacks
  // support of any feature, FB Multi also provides no support of that feature
  f->refill = fbmulti_refill;
  f->seek = fbmulti_seek;

  CLIST_FOR_EACH(struct subbuf *, n, *(FB_MULTI(f)->subbufs))
    {
      ASSERT(n->fb->refill);

      if (!n->fb->seek)
	f->seek = NULL;
    }
}

static void
fbmulti_close(struct fastbuf *f)
{
  CLIST_FOR_EACH(struct subbuf *, n, *(FB_MULTI(f)->subbufs))
    if (n->allow_close)
      bclose(n->fb);

  mp_delete(FB_MULTI(f)->mp);
}

struct fastbuf *
fbmulti_create(uns bufsize, ...)
{
  struct mempool *mp = mp_new(bufsize);
  struct fastbuf *fb_out = mp_alloc(mp, sizeof(struct fb_multi));
  FB_MULTI(fb_out)->mp = mp;

  struct fastbuf *fb_in;
  clist *subbufs = mp_alloc(mp, sizeof(clist));
  clist_init(subbufs);
  FB_MULTI(fb_out)->subbufs = subbufs;

  va_list args;
  va_start(args, bufsize);
  while (fb_in = va_arg(args, struct fastbuf *))
    fbmulti_append(fb_out, fb_in, 1);
  
  va_end(args);

  fbmulti_update_capability(fb_out);

  FB_MULTI(fb_out)->cur = clist_head(subbufs);
  bsetpos(FB_MULTI(fb_out)->cur->fb, 0);

  fbmulti_get_ptrs(fb_out);

  // If seekable, get the length of each subbuf, the total length and boundaries
  if (fb_out->seek)
    {
      fbmulti_get_len(fb_out);
    }

  fb_out->name = FB_MULTI_NAME;

  fb_out->close = fbmulti_close;

  return fb_out;
}

void
fbmulti_append(struct fastbuf *f, struct fastbuf *fb, int allow_close)
{
  struct subbuf *sb = mp_alloc(FB_MULTI(f)->mp, sizeof(struct subbuf));
  sb->fb = fb;
  sb->allow_close = allow_close;
  clist_add_tail(FB_MULTI(f)->subbufs, &(sb->n));
  fbmulti_update_capability(f);
}

void
fbmulti_remove(struct fastbuf *f, struct fastbuf *fb)
{
  bflush(f);
  uns pos = f->pos;
  if (fb)
    {
      CLIST_FOR_EACH(struct subbuf *, n, *(FB_MULTI(f)->subbufs))
	if (fb == n->fb)
	  {
	    // Move the pointers to another buffer if this one was the active.
	    if (FB_MULTI(f)->cur == n)
	      {
		pos = n->begin;
		if (!fbmulti_subbuf_next(f))
		  {
		    struct subbuf *prev = clist_prev(FB_MULTI(f)->subbufs, &FB_MULTI(f)->cur->n);
		    if (prev == NULL)
		      goto cleanup;

		    FB_MULTI(f)->cur = prev;
		    fbmulti_get_ptrs(f);
		  }
	      }

	    if (n->end < pos)
	      pos -= (n->end - n->begin);

	    clist_remove(&(n->n));
	    fbmulti_update_capability(f);
	    fbmulti_get_len(f);
	    fbmulti_get_ptrs(f);
	    return;
	  };

      die("Given fastbuf %p not in given fbmulti %p.", fb, f);
    }
  else
    clist_init(FB_MULTI(f)->subbufs);

cleanup:
  // The fbmulti is empty now, do some cleanup
  fbmulti_update_capability(f);
  fbmulti_get_len(f);
  f->buffer = f->bufend = f->bptr = f->bstop = NULL;
  f->pos = 0;
}

static void fbmulti_flatten_internal(struct fastbuf *f, clist *c, int allow_close)
{
  CLIST_FOR_EACH(struct subbuf *, n, *c)
    {
      if (strcmp(n->fb->name, FB_MULTI_NAME))
	fbmulti_append(f, n->fb, n->allow_close && allow_close);
      
      else
	{
	  fbmulti_flatten_internal(f, FB_MULTI(n->fb)->subbufs, allow_close && n->allow_close);
	  if (allow_close && n->allow_close)
	    {
	      FB_MULTI(n->fb)->subbufs = mp_alloc(FB_MULTI(n->fb)->mp, sizeof(clist));
	      clist_init(FB_MULTI(n->fb)->subbufs);
	      bclose(n->fb);
	    }
	}
    }
}

void
fbmulti_flatten(struct fastbuf *f)
{
  if (strcmp(f->name, FB_MULTI_NAME))
    {
      DBG("fbmulti: given fastbuf isn't fbmulti");
      return;
    }
  
  clist *c = FB_MULTI(f)->subbufs;
  FB_MULTI(f)->subbufs = mp_alloc(FB_MULTI(f)->mp, sizeof(clist));
  clist_init(FB_MULTI(f)->subbufs);

  fbmulti_flatten_internal(f, c, 1);
  FB_MULTI(f)->cur = clist_head(FB_MULTI(f)->subbufs);
  f->bptr = f->bstop = f->buffer;
  f->pos = 0;
}

#ifdef TEST

int main(int argc, char **argv)
{
  if (argc < 2)
    {
      fprintf(stderr, "You must specify a test (r, w, o)\n");
      return 1;
    }
  switch (*argv[1])
    {
      case 'r':
        {
	  char *data[] = { "One\nLine", "Two\nLines", "Th\nreeLi\nnes\n" };
	  struct fastbuf fb[ARRAY_SIZE(data)];
	  for (uns i=0;i<ARRAY_SIZE(data);i++)
	    fbbuf_init_read(&fb[i], data[i], strlen(data[i]), 0);

	  struct fastbuf *f = fbmulti_create(4, &fb[0], &fb[1], &fb[2], NULL);

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
	  for (uns i=0;i<ARRAY_SIZE(data);i++)
	    fbbuf_init_read(&fb[i], data[i], strlen(data[i]), 0);

	  struct fastbuf *f = fbmulti_create(4, &fb[0], &fb[1], NULL);

	  int pos[] = {0, 3, 1, 4, 2, 5};

	  for (uns i=0;i<ARRAY_SIZE(pos);i++)
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

	  struct fastbuf *f = fbmulti_create(8, &fb[0], &fb[1], &fb[2], &fb[1], &fb[3], NULL);

	  char buffer[9];
	  while(bgets(f, buffer, 9))
	    puts(buffer);

	  bclose(f);
	  break;
	}
      case 'f':
      case 'n':
	{
	  char *data[] = { "Nested", "Data", "As", "In", "Real", "Usage", };
	  struct fastbuf fb[ARRAY_SIZE(data)];
	  for (uns i=0;i<ARRAY_SIZE(data);i++)
	    fbbuf_init_read(&fb[i], data[i], strlen(data[i]), 0);

	  struct fastbuf sp;
	  fbbuf_init_read(&sp, " ", 1, 0);

	  struct fastbuf nl;
	  fbbuf_init_read(&nl, "\n", 1, 0);

	  struct fastbuf *f = fbmulti_create(4,
	      fbmulti_create(5,
		&fb[0],
		&sp,
		&fb[1],
		NULL),
	      &nl,
	      fbmulti_create(7,
		&fb[2],
		&sp,
		&fb[3],
		NULL),
	      &nl,
	      fbmulti_create(3,
		&fb[4],
		&sp,
		&fb[5],
		NULL),
	      &nl,
	      NULL);

	  if (*argv[1] == 'f')
	    fbmulti_flatten(f);

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
