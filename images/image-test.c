#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mempool.h"
#include "images/images.h"
#include <string.h>

static uns want_image_iface;

static void
test_image_iface(void)
{
  struct mempool *pool;
  struct image_thread it;
  struct image *i1, *i2;
  struct image s1;
  int i;

  pool = mp_new(1024);
  image_thread_init(&it);

  i1 = image_new(&it, 731, 327, COLOR_SPACE_RGB, NULL);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 3);
  image_destroy(i1);

  i1 = image_new(&it, 2214, 0, COLOR_SPACE_RGB, NULL);
  ASSERT(!i1);

  i1 = image_new(&it, 0xffffff, 0xffffff, COLOR_SPACE_RGB, NULL);
  ASSERT(!i1);

  i1 = image_new(&it, 370, 100, COLOR_SPACE_GRAYSCALE, pool);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 1);
  image_destroy(i1);
  mp_flush(pool);

  i1 = image_new(&it, 373, 101, COLOR_SPACE_RGB | IMAGE_ALIGNED, NULL);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 4);
  ASSERT(IMAGE_SSE_ALIGN_SIZE >= 16);
  ASSERT(!(i1->row_size & (IMAGE_SSE_ALIGN_SIZE - 1)));
  ASSERT(!((addr_int_t)i1->pixels & (IMAGE_SSE_ALIGN_SIZE - 1)));
  image_destroy(i1);

  i1 = image_new(&it, 283, 329, COLOR_SPACE_RGB, NULL);
  ASSERT(i1);
  ASSERT(i1->pixel_size == 3);

  i2 = image_clone(&it, i1, COLOR_SPACE_RGB, NULL);
  ASSERT(i2);
  ASSERT(i2->pixel_size == 3);
  image_destroy(i2);

  i2 = image_clone(&it, i1, COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED, NULL);
  ASSERT(i2);
  ASSERT(i2->pixel_size == 4);
  image_destroy(i2);

  i = image_init_subimage(&it, &s1, i1, 29, 39, 283 - 29, 100);
  ASSERT(i);
  image_destroy(&s1);

  image_destroy(i1);

  image_thread_cleanup(&it);
  mp_delete(pool);
}

int
main(int argc, char **argv)
{
  for (int i = 0; i < argc; i++)
    if (!strcmp(argv[i], "image-iface"))
      want_image_iface = 1;
  if (want_image_iface)
    test_image_iface();
  return 0;
}

