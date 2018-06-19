/*
 *	Image Library -- Simple automatic tests
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/fastbuf.h>
#include <ucw/threads.h>
#include <images/images.h>
#include <images/color.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

static uint want_image_iface;
static uint want_threads;

#define TRY(x) do { if (!(x)) ASSERT(0); } while (0)

static void
test_image_iface(void)
{
  struct mempool *pool;
  struct image_context ctx;
  struct image *i1, *i2;
  struct image s1;

  pool = mp_new(1024);
  image_context_init(&ctx);

  /* Image allocation */
  i1 = image_new(&ctx, 731, 327, COLOR_SPACE_RGB, NULL);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 3);
  image_destroy(i1);

  /* Test invalid image size  */
  ctx.msg_callback = image_context_msg_silent;
  i1 = image_new(&ctx, 2214, 0, COLOR_SPACE_RGB, NULL);
  ASSERT(!i1);
  i1 = image_new(&ctx, 0xffffff, 0xffffff, COLOR_SPACE_RGB, NULL);
  ASSERT(!i1);
  ctx.msg_callback = image_context_msg_default;

  /* Various image allocatio parameters */
  i1 = image_new(&ctx, 370, 100, COLOR_SPACE_GRAYSCALE, pool);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 1);
  image_destroy(i1);
  mp_flush(pool);

  i1 = image_new(&ctx, 373, 101, COLOR_SPACE_RGB | IMAGE_ALIGNED, NULL);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 4);
  ASSERT(IMAGE_SSE_ALIGN_SIZE >= 16);
  ASSERT(!(i1->row_size & (IMAGE_SSE_ALIGN_SIZE - 1)));
  ASSERT(!((uintptr_t)i1->pixels & (IMAGE_SSE_ALIGN_SIZE - 1)));
  image_destroy(i1);

  i1 = image_new(&ctx, 283, 329, COLOR_SPACE_RGB, NULL);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 3);

  /* Image structures cloning */
  i2 = image_clone(&ctx, i1, COLOR_SPACE_RGB, NULL);
  ASSERT(i2);
  ASSERT(i2->pixel_size == 3);
  image_destroy(i2);

  i2 = image_clone(&ctx, i1, COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED, NULL);
  ASSERT(i2);
  ASSERT(i2->pixel_size == 4);
  image_destroy(i2);

  /* Subimages */
  i2 = image_init_subimage(&ctx, &s1, i1, 29, 39, 283 - 29, 100);
  ASSERT(i2);
  image_destroy(&s1);

  image_destroy(i1);

  image_context_cleanup(&ctx);
  mp_delete(pool);
}

#ifdef CONFIG_UCW_THREADS

#define TEST_THREADS_COUNT 4

static void *
test_threads_thread(void *param UNUSED)
{
  DBG("Starting thread");
  struct image_context ctx;
  struct image_io io;
  image_context_init(&ctx);
  TRY(image_io_init(&ctx, &io));

  for (uint num = 0; num < 200; num++)
    {
      int r0 = random_max(100);

      /* realloc context */
      if ((r0 -= 2) < 0)
        {
	  image_io_cleanup(&io);
	  image_context_cleanup(&ctx);
	  image_context_init(&ctx);
	  TRY(image_io_init(&ctx, &io));
	}

      /* realloc I/O */
      else if ((r0 -= 2) < 0)
        {
	  image_io_cleanup(&io);
	  TRY(image_io_init(&ctx, &io));
	}

      /* encode and decode random image */
      else
        {
          struct image *img;

          TRY(img = image_new(&ctx, 10 + random_max(140), 10 + random_max(140), COLOR_SPACE_RGB, NULL));
	  image_clear(&ctx, img);

#if defined(CONFIG_IMAGES_LIBJPEG) || defined(CONFIG_IMAGES_LIBPNG) || defined(CONFIG_IMAGES_LIBMAGICK)

	  struct fastbuf *wfb = fbmem_create(10000);
	  struct fastbuf *rfb;
	  uint format = 0;
	  while (!format)
	    {
	      switch (random_max(3))
	        {
		  case 0:
#if defined(CONFIG_IMAGES_LIBJPEG) || defined(CONFIG_IMAGES_LIBMAGICK)
		    format = IMAGE_FORMAT_JPEG;
#endif
		    break;
		  case 1:
#if defined(CONFIG_IMAGES_LIBPNG) || defined(CONFIG_IMAGES_LIBMAGICK)
		    format = IMAGE_FORMAT_PNG;
#endif
		    break;
		  case 2:
#if defined(CONFIG_IMAGES_LIBMAGICK)
		    format = IMAGE_FORMAT_GIF;
#endif
		    break;
		  default:
		    ASSERT(0);
		}
	    }

	  io.format = format;
	  io.fastbuf = wfb;
	  io.image = img;
	  TRY(image_io_write(&io));
	  image_io_reset(&io);

	  rfb = fbmem_clone_read(wfb);
	  io.format = format;
	  io.fastbuf = rfb;
	  TRY(image_io_read(&io, 0));
	  image_io_reset(&io);

	  bclose(rfb);
	  bclose(wfb);

#endif
          image_destroy(img);
	}
    }

  image_io_cleanup(&io);
  image_context_cleanup(&ctx);
  DBG("Stopping thread");
  return NULL;
}

#endif

static void
test_threads(void)
{
#ifdef CONFIG_UCW_THREADS
  pthread_t threads[TEST_THREADS_COUNT - 1];
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) < 0 ||
      pthread_attr_setstacksize(&attr, ucwlib_thread_stack_size) < 0)
    ASSERT(0);
  for (uint i = 0; i < TEST_THREADS_COUNT - 1; i++)
    {
      if (pthread_create(threads + i, &attr, test_threads_thread, NULL) < 0)
        die("Unable to create thread: %m");
    }
  test_threads_thread(NULL);
  for (uint i = 0; i < TEST_THREADS_COUNT - 1; i++)
    if (pthread_join(threads[i], NULL) < 0)
      die("Cannot join thread: %m");
#else
  msg(L_WARN, "Disabled CONFIG_UCW_THREADS, threaded tests skipped");
#endif
}

int
main(int argc, char **argv)
{
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "image-iface"))
      want_image_iface++;
    else if (!strcmp(argv[i], "threads"))
      want_threads++;
    else
      die("Invalid parameter");

  srandom(time(NULL) ^ getpid());

  if (want_image_iface)
    test_image_iface();
  if (want_threads)
    test_threads();

  return 0;
}

