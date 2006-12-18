/*
 *	The UCW Library -- Threading Helpers
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/threads.h"

#ifdef CONFIG_UCW_THREADS

#include <pthread.h>

static pthread_key_t ucwlib_context_key;
static pthread_mutex_t ucwlib_master_mutex;

static void
ucwlib_free_thread_context(void *p)
{
  xfree(p);
}

static void CONSTRUCTOR
ucwlib_threads_init(void)
{
  if (pthread_key_create(&ucwlib_context_key, ucwlib_free_thread_context) < 0)
    die("Cannot create pthread_key: %m");
  pthread_mutex_init(&ucwlib_master_mutex, NULL);
}

static int
ucwlib_tid(void)
{
  static tid_counter;

  ucwlib_lock();
  int tid = ++tid_counter;
  ucwlib_unlock();
  return tid;
}

struct ucwlib_context *
ucwlib_thread_context(void)
{
  struct ucwlib_context *c = pthread_getspecific(ucwlib_context_key);
  if (!c)
    {
      c = xmalloc_zero(sizeof(*c));
      c->thread_id = ucwlib_tid();
      pthread_setspecific(ucwlib_context_key, c);
    }
  return c;
}

void
ucwlib_lock(void)
{
  pthread_mutex_lock(&ucwlib_master_mutex);
}

void
ucwlib_unlock(void)
{
  pthread_mutex_unlock(&ucwlib_master_mutex);
}

#else

struct ucwlib_context *
ucwlib_thread_context(void)
{
  static struct ucwlib_context ucwlib_context;
  return &ucwlib_context;
}

void
ucwlib_lock(void)
{
}

void
ucwlib_unlock(void)
{
}

#endif

#ifdef TEST

int main(void)
{
  ucwlib_lock();
  ucwlib_unlock();
  log(L_INFO, "tid=%d", ucwlib_thread_context()->thread_id);
  return 0;
}

#endif
