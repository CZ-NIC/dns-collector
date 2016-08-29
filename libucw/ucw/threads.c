/*
 *	The UCW Library -- Threading Helpers
 *
 *	(c) 2006--2010 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/threads.h>

#ifdef CONFIG_UCW_THREADS

#include <pthread.h>

#ifdef CONFIG_LINUX
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifdef __NR_gettid
static pid_t
gettid(void)
{
  return syscall(__NR_gettid);
}
#define CONFIG_USE_GETTID
#endif
#endif

/*** Library lock ***/

static pthread_mutex_t ucwlib_master_mutex;

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

/*** Thread identifiers ***/

static int
ucwlib_tid(void)
{
  static int tid_counter;
  int tid;

#ifdef CONFIG_USE_GETTID
  tid = gettid();
  if (tid > 0)
    return tid;
  /* The syscall might be unimplemented */
#endif

  ucwlib_lock();
  tid = ++tid_counter;
  ucwlib_unlock();
  return tid;
}

/*** Thread context ***/

static void CONSTRUCTOR_WITH_PRIORITY(10000)
ucwlib_threads_init_master(void)
{
  pthread_mutex_init(&ucwlib_master_mutex, NULL);
}

#ifdef CONFIG_UCW_TLS

__thread struct ucwlib_context ucwlib_context;

int
ucwlib_thread_id(struct ucwlib_context *c)
{
  if (!c->_thread_id)
    c->_thread_id = ucwlib_tid();
  return c->_thread_id;
}

#else

static pthread_key_t ucwlib_context_key;

static void
ucwlib_free_thread_context(void *p)
{
  xfree(p);
}

static void CONSTRUCTOR_WITH_PRIORITY(10000)
ucwlib_threads_init(void)
{
  if (pthread_key_create(&ucwlib_context_key, ucwlib_free_thread_context) < 0)
    die("Cannot create pthread_key: %m");
}

struct ucwlib_context *
ucwlib_thread_context(void)
{
  struct ucwlib_context *c = pthread_getspecific(ucwlib_context_key);
  if (!c)
    {
      c = xmalloc_zero(sizeof(*c));
      c->_thread_id = ucwlib_tid();
      pthread_setspecific(ucwlib_context_key, c);
    }
  return c;
}

#endif /* CONFIG_UCW_TLS */

#else /* !CONFIG_UCW_THREADS */

struct ucwlib_context ucwlib_default_context;

#endif

#ifdef TEST

int main(void)
{
  ucwlib_lock();
  ucwlib_unlock();
  msg(L_INFO, "tid=%d", ucwlib_thread_id(ucwlib_thread_context()));
  return 0;
}

#endif
