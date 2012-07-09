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

struct fb_multi {
  struct fastbuf fb;
  struct mempool* mp;
  struct subbuf* cur;
  ucw_off_t len;
  clist* subbufs;
};
#define FB_MULTI(f) ((struct fb_multi *)(f))

struct subbuf {
  cnode n;
  ucw_off_t begin, end;
  struct fastbuf* fb;
};
#define SUBBUF(f) ((struct subbuf *)(f))

static inline void
fbmulti_subbuf_get_end(struct subbuf *s)
{
  if (s->fb->seek) {
    bseek(s->fb, 0, SEEK_END);
    s->end = s->begin + btell(s->fb);
  }
}

static inline int
fbmulti_subbuf_next(struct fastbuf *f)
{
  struct subbuf* next = clist_next(FB_MULTI(f)->subbufs, &FB_MULTI(f)->cur->n);
  if (next == NULL)
    return 0;
  
  if (f->seek) {
    bseek(FB_MULTI(f)->cur->fb, 0, SEEK_SET);
    next->begin = FB_MULTI(f)->cur->end;
  }

  FB_MULTI(f)->cur = next;
  return 1;
}

static int
fbmulti_refill(struct fastbuf *f)
{
  if (f->bufend == f->bstop)
    f->bptr = f->bstop = f->buffer;
  uns len = bread(FB_MULTI(f)->cur->fb, f->bstop, (f->bufend - f->bstop));
  f->bstop += len;
  f->pos += len;
  if (len)
    return len;

  // Current buf returned EOF
  // Update the information on end of this buffer
  fbmulti_subbuf_get_end(FB_MULTI(f)->cur);

  // Take the next one if exists
  if (fbmulti_subbuf_next(f))
    return fbmulti_refill(f);
  else
    return 0;
}

static void
fbmulti_get_len(struct fastbuf *f)
{
  ASSERT (f->seek);
  FB_MULTI(f)->len = 0;
  
  CLIST_FOR_EACH(struct subbuf *, n, *(FB_MULTI(f)->subbufs))
    {
      n->begin = FB_MULTI(f)->len;
      fbmulti_subbuf_get_end(n);
      FB_MULTI(f)->len = n->end;
    }
}

static int
fbmulti_seek(struct fastbuf *f, ucw_off_t pos, int whence)
{
  switch(whence)
    {
    case SEEK_SET:
      if (f->pos > pos) {
	FB_MULTI(f)->cur = clist_head(FB_MULTI(f)->subbufs);
	FB_MULTI(f)->cur->begin = 0;
	f->pos = 0;
	return fbmulti_seek(f, pos, SEEK_SET);
      }

      do {
	fbmulti_subbuf_get_end(FB_MULTI(f)->cur);
	if (pos < FB_MULTI(f)->cur->end)
	  break;

	if (!fbmulti_subbuf_next(f))
	  bthrow(f, "seek", "Seek out of range");

      } while (1);

      bseek(FB_MULTI(f)->cur->fb, (pos - FB_MULTI(f)->cur->begin), SEEK_SET);
      f->pos = pos;
      f->bptr = f->bstop = f->buffer;
      return 1;
      break;

    case SEEK_END:
      fbmulti_get_len(f);
      return fbmulti_seek(f, FB_MULTI(f)->len+pos, SEEK_CUR);
      break;

    default:
      ASSERT(0);
    }
}

static void
fbmulti_update_capability(struct fastbuf *f) {
  // FB Multi is only a proxy to other fastbufs ... if any of them lacks
  // support of any feature, FB Multi also provides no support of that feature
  f->refill = fbmulti_refill;
  f->seek = fbmulti_seek;

  CLIST_FOR_EACH(struct subbuf *, n, *(FB_MULTI(f)->subbufs)) {
    if (!n->fb->refill)
      f->refill = NULL;

    if (!n->fb->seek)
      f->seek = NULL;
  }
}

struct fastbuf*
fbmulti_create(uns bufsize, ...)
{
  struct mempool *mp = mp_new(bufsize);
  struct fastbuf *fb_out = mp_alloc(mp, sizeof(struct fb_multi));
  FB_MULTI(fb_out)->mp = mp;

  struct fastbuf *fb_in;
  clist* subbufs = mp_alloc(mp, sizeof(clist));
  clist_init(subbufs);
  FB_MULTI(fb_out)->subbufs = subbufs;

  va_list args;
  va_start(args, bufsize);
  while (fb_in = va_arg(args, struct fastbuf *)) {
    struct subbuf *sb = mp_alloc(mp, sizeof(struct subbuf));
    sb->fb = fb_in;
    clist_add_tail(subbufs, &(sb->n));
  }
  va_end(args);

  FB_MULTI(fb_out)->cur = clist_head(subbufs);

  fb_out->buffer = mp_alloc(mp, bufsize);
  fb_out->bptr = fb_out->bstop = fb_out->buffer;
  fb_out->bufend = fb_out->buffer + bufsize;
  fb_out->name = "<multi>";

  fbmulti_update_capability(fb_out);

  return fb_out;
}

#ifdef TEST

int main(int argc, char ** argv)
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

	  struct fastbuf* f = fbmulti_create(4, &fb[0], &fb[1], &fb[2], NULL);

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

	  struct fastbuf* f = fbmulti_create(4, &fb[0], &fb[1], NULL);

	  int pos[] = {0, 3, 1, 4, 2, 5};

	  for (uns i=0;i<ARRAY_SIZE(pos);i++) {
	    bseek(f, pos[i], SEEK_SET);
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

	  struct fastbuf* f = fbmulti_create(8, &fb[0], &fb[1], &fb[2], &fb[1], &fb[3], NULL);

	  char buffer[9];
	  while(bgets(f, buffer, 9))
	    puts(buffer);

	  bclose(f);
	  break;
	}
    }
  return 0;
}

#endif
