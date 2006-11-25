/*
 *	UCW Library -- Thread Pools and Work Queues
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/workqueue.h"

static void *
worker_thread_init(void *arg)
{
  struct worker_thread *t = arg;
  struct worker_pool *pool = t->pool;

  if (pool->init_thread)
    pool->init_thread(t);
  sem_post(pool->init_cleanup_sem);

  for (;;)
    {
      struct work *w = raw_queue_get(&pool->requests);
      w->go(t, w);
      raw_queue_put(&w->reply_to->finished, w);
    }

  return NULL;
}

static void
worker_thread_signal_finish(struct worker_thread *t, struct work *w UNUSED)
{
  if (t->pool->cleanup_thread)
    t->pool->cleanup_thread(t);
  sem_post(t->pool->init_cleanup_sem);
  pthread_exit(NULL);
}

void
worker_pool_init(struct worker_pool *p)
{
  clist_init(&p->worker_threads);
  raw_queue_init(&p->requests);
  p->init_cleanup_sem = sem_alloc();

  pthread_attr_t attr;
  if (pthread_attr_init(&attr) < 0 ||
      pthread_attr_setstacksize(&attr, p->stack_size) < 0)
    ASSERT(0);

  for (uns i=0; i < p->num_threads; i++)
    {
      struct worker_thread *t = (p->new_thread ? p->new_thread() : xmalloc(sizeof(*t)));
      t->pool = p;
      t->id = i;
      int err = pthread_create(&t->thread, &attr, worker_thread_init, t);
      if (err)
	die("Unable to create thread: %m");
      clist_add_tail(&p->worker_threads, &t->n);
      sem_wait(p->init_cleanup_sem);
    }

  pthread_attr_destroy(&attr);
}

void
worker_pool_cleanup(struct worker_pool *p)
{
  for (uns i=0; i < p->num_threads; i++)
    {
      struct work w = {
	.go = worker_thread_signal_finish
      };
      raw_queue_put(&p->requests, &w);
      sem_wait(p->init_cleanup_sem);
    }

  struct worker_thread *tmp;
  CLIST_FOR_EACH_DELSAFE(struct worker_thread *, t, p->worker_threads, tmp)
    {
      int err = pthread_join(t->thread, NULL);
      ASSERT(!err);
      if (p->free_thread)
	p->free_thread(t);
      else
	xfree(t);
    }
  raw_queue_cleanup(&p->requests);
  sem_free(p->init_cleanup_sem);
}

void
raw_queue_init(struct raw_queue *q)
{
  pthread_mutex_init(&q->queue_mutex, NULL);
  clist_init(&q->queue);
  q->queue_sem = sem_alloc();
}

void
raw_queue_cleanup(struct raw_queue *q)
{
  ASSERT(clist_empty(&q->queue));
  sem_free(q->queue_sem);
  pthread_mutex_destroy(&q->queue_mutex);
}

void
raw_queue_put(struct raw_queue *q, struct work *w)
{
  pthread_mutex_lock(&q->queue_mutex);
  clist_add_tail(&q->queue, &w->n);
  pthread_mutex_unlock(&q->queue_mutex);
  sem_post(q->queue_sem);
}

struct work *
raw_queue_get(struct raw_queue *q)
{
  sem_wait(q->queue_sem);
  pthread_mutex_lock(&q->queue_mutex);
  struct work *w = clist_head(&q->queue);
  ASSERT(w);
  clist_remove(&w->n);
  pthread_mutex_unlock(&q->queue_mutex);
  return w;
}

void
work_queue_init(struct worker_pool *p, struct work_queue *q)
{
  q->pool = p;
  q->nr_running = 0;
  raw_queue_init(&q->finished);
}

void
work_queue_cleanup(struct work_queue *q)
{
  ASSERT(!q->nr_running);
  raw_queue_cleanup(&q->finished);
}

void
work_submit(struct work_queue *q, struct work *w)
{
  ASSERT(w->go);
  w->reply_to = q;
  raw_queue_put(&q->pool->requests, w);
  q->nr_running++;
}

struct work *
work_wait(struct work_queue *q)
{
  while (q->nr_running)
    {
      struct work *w = raw_queue_get(&q->finished);
      q->nr_running--;
      if (w->returned)
	w->returned(q, w);
      else
	return w;
    }
  return NULL;
}

#ifdef TEST

#include <unistd.h>

static void wt_init(struct worker_thread *t)
{
  log(L_INFO, "INIT %d", t->id);
}

static void wt_cleanup(struct worker_thread *t)
{
  log(L_INFO, "CLEANUP %d", t->id);
}

struct w {
  struct work w;
  uns id;
};

static void go(struct worker_thread *t, struct work *w)
{
  log(L_INFO, "GO %d: request %d", t->id, ((struct w *)w)->id);
  usleep(1);
}

int main(void)
{
  struct worker_pool pool = {
    .num_threads = 10,
    .stack_size = 65536,
    .init_thread = wt_init,
    .cleanup_thread = wt_cleanup
  };
  worker_pool_init(&pool);

  struct work_queue q;
  work_queue_init(&pool, &q);
  for (uns i=0; i<500; i++)
    {
      struct w *w = xmalloc_zero(sizeof(*w));
      w->w.go = go;
      w->id = i;
      work_submit(&q, &w->w);
    }

  struct w *w;
  while (w = (struct w *) work_wait(&q))
    log(L_INFO, "Finished request %d", w->id);

  work_queue_cleanup(&q);
  worker_pool_cleanup(&pool);
  return 0;
}

#endif
