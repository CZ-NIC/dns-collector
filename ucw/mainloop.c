/*
 *	UCW Library -- Main Loop
 *
 *	(c) 2004--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/heap.h>
#include <ucw/mainloop.h>
#include <ucw/threads.h>
#include <ucw/gary.h>
#include <ucw/process.h>
#include <ucw/time.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/time.h>

#ifdef CONFIG_UCW_THREADS
#include <pthread.h>
#define THREAD_SIGMASK pthread_sigmask
#else
#define THREAD_SIGMASK sigprocmask
#endif

#ifdef CONFIG_UCW_EPOLL
#include <sys/epoll.h>
#endif

#define MAIN_TIMER_LESS(x,y) ((x)->expires < (y)->expires)
#define MAIN_TIMER_SWAP(heap,a,b,t) (t=heap[a], heap[a]=heap[b], heap[b]=t, heap[a]->index=(a), heap[b]->index=(b))

#define EPOLL_BUF_SIZE 256

static void file_del_ctx(struct main_context *m, struct main_file *fi);
static void signal_del_ctx(struct main_context *m, struct main_signal *ms);

static void
main_get_time_ctx(struct main_context *m)
{
  m->now = get_timestamp();
}

static struct main_context *
main_current_nocheck(void)
{
  return ucwlib_thread_context()->main_context;
}

struct main_context *
main_current(void)
{
  struct main_context *m = main_current_nocheck();
  ASSERT(m);
  return m;
}

static int
main_is_current(struct main_context *m)
{
  return (m == main_current_nocheck());
}

static inline uns
count_timers(struct main_context *m)
{
  if (m->timer_table)
    return GARY_SIZE(m->timer_table) - 1;
  else
    return 0;
}

struct main_context *
main_new(void)
{
  struct main_context *m = xmalloc_zero(sizeof(*m));

  DBG("MAIN: New context");
  clist_init(&m->file_list);
  clist_init(&m->file_active_list);
  clist_init(&m->hook_list);
  clist_init(&m->hook_done_list);
  clist_init(&m->process_list);
  clist_init(&m->signal_list);
#ifdef CONFIG_UCW_EPOLL
  m->epoll_fd = epoll_create(64);
  if (m->epoll_fd < 0)
    die("epoll_create() failed: %m");
  m->epoll_events = xmalloc(EPOLL_BUF_SIZE * sizeof(struct epoll_event));
  clist_init(&m->file_recalc_list);
#else
  m->poll_table_obsolete = 1;
#endif
  main_get_time_ctx(m);
  sigemptyset(&m->want_signals);
  m->sig_pipe_recv = m->sig_pipe_send = -1;

  return m;
}

static void
main_prepare_delete(struct main_context *m)
{
  /*
   *  If the context is current, deactivate it first. But beware,
   *  we must not call functions that depend on the current context.
   */
  if (main_is_current(m))
    main_switch_context(NULL);

  // Close epoll descriptor early enough, it might be shared after fork!
#ifdef CONFIG_UCW_EPOLL
  xfree(m->epoll_events);
  close(m->epoll_fd);
  m->epoll_fd = -1;
#else
  GARY_FREE(m->poll_table);
  GARY_FREE(m->poll_file_table);
#endif

  if (m->sigchld_handler)
    {
      signal_del_ctx(m, m->sigchld_handler);
      xfree(m->sigchld_handler);
    }
  if (m->sig_pipe_file)
    {
      file_del_ctx(m, m->sig_pipe_file);
      xfree(m->sig_pipe_file);
     }
  if (m->sig_pipe_recv >= 0)
    {
      close(m->sig_pipe_recv);
      close(m->sig_pipe_send);
    }
}

static void
main_do_delete(struct main_context *m)
{
  GARY_FREE(m->timer_table);
  xfree(m);
}

void
main_delete(struct main_context *m)
{
  if (!m)
    return;

  main_prepare_delete(m);
  ASSERT(clist_empty(&m->file_list));
  ASSERT(clist_empty(&m->file_active_list));
#ifdef CONFIG_UCW_EPOLL
  ASSERT(clist_empty(&m->file_recalc_list));
#endif
  ASSERT(clist_empty(&m->hook_list));
  ASSERT(clist_empty(&m->hook_done_list));
  ASSERT(clist_empty(&m->process_list));
  ASSERT(clist_empty(&m->signal_list));
  ASSERT(!count_timers(m));
  main_do_delete(m);
}

void
main_destroy(struct main_context *m)
{
  if (!m)
    return;
  main_prepare_delete(m);

  // Close all files
  clist_insert_list_after(&m->file_active_list, m->file_list.head.prev);
#ifdef CONFIG_UCW_EPOLL
  clist_insert_list_after(&m->file_recalc_list, m->file_list.head.prev);
#endif
  CLIST_FOR_EACH(struct main_file *, f, m->file_list)
    close(f->fd);

  main_do_delete(m);
}

struct main_context *
main_switch_context(struct main_context *m)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  struct main_context *m0 = c->main_context;

  /*
   *  Not only we need to switch the signal sets of the two contexts,
   *  but it is also necessary to avoid invoking a signal handler
   *  in the middle of changing c->main_context.
   */
  if (m0 && !clist_empty(&m0->signal_list))
    THREAD_SIGMASK(SIG_BLOCK, &m0->want_signals, NULL);
  c->main_context = m;
  if (m && !clist_empty(&m->signal_list))
    THREAD_SIGMASK(SIG_UNBLOCK, &m->want_signals, NULL);

  return m0;
}

void
main_init(void)
{
  struct main_context *m = main_switch_context(main_new());
  ASSERT(!m);
}

void
main_cleanup(void)
{
  main_delete(main_current_nocheck());
}

void
main_teardown(void)
{
  main_destroy(main_current_nocheck());
}

void
main_get_time(void)
{
  main_get_time_ctx(main_current());
}

void
timer_add(struct main_timer *tm, timestamp_t expires)
{
  struct main_context *m = main_current();

  if (!m->timer_table)
    {
      GARY_INIT(m->timer_table, 1);
      m->timer_table[0] = NULL;
    }

  if (expires)
    DBG("MAIN: Setting timer %p (expire at now+%lld)", tm, (long long)(expires - m->now));
  else
    DBG("MAIN: Clearing timer %p", tm);
  uns num_timers = count_timers(m);
  if (tm->expires < expires)
    {
      if (!tm->expires)
	{
	  tm->expires = expires;
	  tm->index = num_timers + 1;
	  GARY_RESIZE(m->timer_table, num_timers + 2);
	  HEAP_INSERT(struct main_timer *, m->timer_table, num_timers, MAIN_TIMER_LESS, MAIN_TIMER_SWAP, tm);
	}
      else
	{
	  tm->expires = expires;
	  HEAP_INCREASE(struct main_timer *, m->timer_table, num_timers, MAIN_TIMER_LESS, MAIN_TIMER_SWAP, tm->index, tm);
	}
    }
  else if (tm->expires > expires)
    {
      if (!expires)
	{
	  ASSERT(tm->index && tm->index <= num_timers);
	  HEAP_DELETE(struct main_timer *, m->timer_table, num_timers, MAIN_TIMER_LESS, MAIN_TIMER_SWAP, tm->index);
	  tm->index = 0;
	  tm->expires = 0;
	  GARY_POP(m->timer_table);
	}
      else
	{
	  tm->expires = expires;
	  HEAP_DECREASE(struct main_timer *, m->timer_table, num_timers, MAIN_TIMER_LESS, MAIN_TIMER_SWAP, tm->index, tm);
	}
    }
}

void
timer_add_rel(struct main_timer *tm, timestamp_t expires_delta)
{
  struct main_context *m = main_current();
  return timer_add(tm, m->now + expires_delta);
}

void
timer_del(struct main_timer *tm)
{
  timer_add(tm, 0);
}

static uns
file_want_events(struct main_file *fi)
{
  uns events = 0;
  if (fi->read_handler)
    events |= POLLIN;
  if (fi->write_handler)
    events |= POLLOUT;
  return events;
}

void
file_add(struct main_file *fi)
{
  struct main_context *m = main_current();

  DBG("MAIN: Adding file %p (fd=%d)", fi, fi->fd);
  ASSERT(!file_is_active(fi));
  clist_add_tail(&m->file_list, &fi->n);
  m->file_cnt++;
#ifdef CONFIG_UCW_EPOLL
  struct epoll_event evt = {
    .events = file_want_events(fi),
    .data.ptr = fi,
  };
  if (epoll_ctl(m->epoll_fd, EPOLL_CTL_ADD, fi->fd, &evt) < 0)
    die("epoll_ctl() failed: %m");
  fi->last_want_events = evt.events;
#else
  m->poll_table_obsolete = 1;
#endif
  if (fcntl(fi->fd, F_SETFL, O_NONBLOCK) < 0)
    msg(L_ERROR, "Error setting fd %d to non-blocking mode: %m. Keep fingers crossed.", fi->fd);
}

void
file_chg(struct main_file *fi)
{
#ifdef CONFIG_UCW_EPOLL
  clist_remove(&fi->n);
  clist_add_tail(&main_current()->file_recalc_list, &fi->n);
#else
  struct pollfd *p = fi->pollfd;
  if (p)
    p->events = file_want_events(fi);
#endif
}

static void
file_del_ctx(struct main_context *m, struct main_file *fi)
{
  // XXX: Can be called on a non-current context
  DBG("MAIN: Deleting file %p (fd=%d)", fi, fi->fd);

  if (!file_is_active(fi))
    return;
  clist_unlink(&fi->n);
  m->file_cnt--;
#ifdef CONFIG_UCW_EPOLL
  if (m->epoll_fd >= 0 && epoll_ctl(m->epoll_fd, EPOLL_CTL_DEL, fi->fd, NULL) < 0)
    die("epoll_ctl() failed: %m");
#else
  m->poll_table_obsolete = 1;
#endif
}

void
file_del(struct main_file *fi)
{
  file_del_ctx(main_current(), fi);
}

void
hook_add(struct main_hook *ho)
{
  struct main_context *m = main_current();

  DBG("MAIN: Adding hook %p", ho);
  if (hook_is_active(ho))
    clist_unlink(&ho->n);
  clist_add_tail(&m->hook_list, &ho->n);
}

void
hook_del(struct main_hook *ho)
{
  DBG("MAIN: Deleting hook %p", ho);
  if (hook_is_active(ho))
    clist_unlink(&ho->n);
}

static void
sigchld_received(struct main_signal *sg UNUSED)
{
  struct main_context *m = main_current();
  int stat;
  pid_t pid;

  while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
      DBG("MAIN: Child %d exited with status %x", pid, stat);
      CLIST_FOR_EACH(struct main_process *, pr, m->process_list)
	if (pr->pid == pid)
	  {
	    pr->status = stat;
	    process_del(pr);
	    format_exit_status(pr->status_msg, pr->status);
	    DBG("MAIN: Calling process exit handler");
	    pr->handler(pr);
	    break;
	  }
    }
}

void
process_add(struct main_process *mp)
{
  struct main_context *m = main_current();

  DBG("MAIN: Adding process %p (pid=%d)", mp, mp->pid);
  ASSERT(!process_is_active(mp));
  ASSERT(mp->handler);
  clist_add_tail(&m->process_list, &mp->n);
  if (!m->sigchld_handler)
    {
      struct main_signal *sg = xmalloc_zero(sizeof(*sg));
      m->sigchld_handler = sg;
      sg->signum = SIGCHLD;
      sg->handler = sigchld_received;
      signal_add(sg);
    }
}

void
process_del(struct main_process *mp)
{
  DBG("MAIN: Deleting process %p (pid=%d)", mp, mp->pid);
  if (process_is_active(mp))
    clist_unlink(&mp->n);
}

int
process_fork(struct main_process *mp)
{
  pid_t pid = fork();
  if (pid < 0)
    {
      DBG("MAIN: Fork failed");
      mp->status = -1;
      format_exit_status(mp->status_msg, -1);
      mp->handler(mp);
      return 1;
    }
  else if (!pid)
    return 0;
  else
    {
      DBG("MAIN: Forked process %d", (int) pid);
      mp->pid = pid;
      process_add(mp);
      return 1;
    }
}

static int
pipe_read_handler(struct main_file *mf UNUSED)
{
  struct main_context *m = main_current();
  int signum;
  int n = read(m->sig_pipe_recv, &signum, sizeof(signum));

  if (n < 0)
    {
      if (errno != EAGAIN)
	msg(L_ERROR, "Error reading signal pipe: %m");
      return 0;
    }
  ASSERT(n == sizeof(signum));

  DBG("MAIN: Sigpipe: received signal %d", signum);
  struct main_signal iter = { .signum = -1 };
  struct main_signal *sg = clist_head(&m->signal_list);
  while (sg)
    {
      if (sg->signum == signum)
	{
	  DBG("MAIN: Sigpipe: invoking handler %p", sg);
	  clist_insert_after(&iter.n, &sg->n);
	  sg->handler(sg);
	  sg = clist_next(&m->signal_list, &iter.n);
	  clist_remove(&iter.n);
	}
      else
	sg = clist_next(&m->signal_list, &sg->n);
    }

  return 1;
}

static void
pipe_configure(int fd)
{
  int flags;
  if ((flags = fcntl(fd, F_GETFL)) < 0 || fcntl(fd, F_SETFL, flags|O_NONBLOCK) < 0)
    die("Could not set file descriptor %d to non-blocking: %m", fd);
}

static void
pipe_setup(struct main_context *m)
{
  DBG("MAIN: Sigpipe: Setting up the pipe");

  int pipe_result[2];
  if (pipe(pipe_result) == -1)
    die("Could not create signal pipe: %m");
  pipe_configure(pipe_result[0]);
  pipe_configure(pipe_result[1]);
  m->sig_pipe_recv = pipe_result[0];
  m->sig_pipe_send = pipe_result[1];

  struct main_file *f = xmalloc_zero(sizeof(*f));
  m->sig_pipe_file = f;
  f->fd = m->sig_pipe_recv;
  f->read_handler = pipe_read_handler;
  file_add(f);
}

static void
signal_handler_pipe(int signum)
{
  struct main_context *m = main_current();
#ifdef LOCAL_DEBUG
  msg(L_DEBUG | L_SIGHANDLER, "MAIN: Sigpipe: sending signal %d down the drain", signum);
#endif
  write(m->sig_pipe_send, &signum, sizeof(signum));
}

void
signal_add(struct main_signal *ms)
{
  struct main_context *m = main_current();

  DBG("MAIN: Adding signal %p (sig=%d)", ms, ms->signum);

  ASSERT(!signal_is_active(ms));
  // Adding at the head of the list is better if we are in the middle of walking the list.
  clist_add_head(&m->signal_list, &ms->n);
  if (m->sig_pipe_recv < 0)
    pipe_setup(m);

  struct sigaction sa = {
    .sa_handler = signal_handler_pipe,
    .sa_flags = SA_NOCLDSTOP | SA_RESTART,
  };
  sigaction(ms->signum, &sa, NULL);

  sigset_t ss;
  sigemptyset(&ss);
  sigaddset(&ss, ms->signum);
  THREAD_SIGMASK(SIG_UNBLOCK, &ss, NULL);
  sigaddset(&m->want_signals, ms->signum);
}

static void
signal_del_ctx(struct main_context *m, struct main_signal *ms)
{
  // XXX: Can be called on a non-current context
  DBG("MAIN: Deleting signal %p (sig=%d)", ms, ms->signum);

  if (!signal_is_active(ms))
    return;
  clist_unlink(&ms->n);

  int another = 0;
  CLIST_FOR_EACH(struct main_signal *, s, m->signal_list)
    if (s->signum == ms->signum)
      another++;
  if (!another)
    {
      if (main_is_current(m))
	{
	  sigset_t ss;
	  sigemptyset(&ss);
	  sigaddset(&ss, ms->signum);
	  THREAD_SIGMASK(SIG_BLOCK, &ss, NULL);
	}
      sigdelset(&m->want_signals, ms->signum);
    }
}

void
signal_del(struct main_signal *ms)
{
  signal_del_ctx(main_current(), ms);
}

#ifdef CONFIG_UCW_DEBUG

void
file_debug(struct main_file *fi)
{
  msg(L_DEBUG, "\t\t%p (fd %d, rh %p, wh %p, data %p)",
    fi, fi->fd, fi->read_handler, fi->write_handler, fi->data);
}

void
hook_debug(struct main_hook *ho)
{
  msg(L_DEBUG, "\t\t%p (func %p, data %p)", ho, ho->handler, ho->data);
}

void
signal_debug(struct main_signal *sg)
{
  if (sg->signum < 0)
    msg(L_DEBUG, "\t\t(placeholder)");
  else
    msg(L_DEBUG, "\t\t%p (sig %d, func %p, data %p)", sg, sg->signum, sg->handler, sg->data);
}

static void
timer_debug_ctx(struct main_context *m, struct main_timer *tm)
{
  msg(L_DEBUG, "\t\t%p (expires %lld, data %p)", tm, (long long)(tm->expires - m->now), tm->data);
}

void
timer_debug(struct main_timer *tm)
{
  timer_debug_ctx(main_current(), tm);
}

void
process_debug(struct main_process *pr)
{
  msg(L_DEBUG, "\t\t%p (pid %d, func %p, data %p)", pr, pr->pid, pr->handler, pr->data);
}

void
main_debug_context(struct main_context *m UNUSED)
{
  msg(L_DEBUG, "### Main loop status on %lld", (long long) m->now);
  msg(L_DEBUG, "\tActive timers:");
  uns num_timers = count_timers(m);
  for (uns i = 1; i <= num_timers; i++)
    timer_debug(m->timer_table[i]);
  msg(L_DEBUG, "\tActive files:");
  CLIST_FOR_EACH(struct main_file *, fi, m->file_list)
    file_debug(fi);
  CLIST_FOR_EACH(struct main_file *, fi, m->file_active_list)
    file_debug(fi);
#ifdef CONFIG_UCW_EPOLL
  CLIST_FOR_EACH(struct main_file *, fi, m->file_recalc_list)
    file_debug(fi);
#endif
  msg(L_DEBUG, "\tActive hooks:");
  CLIST_FOR_EACH(struct main_hook *, ho, m->hook_done_list)
    hook_debug(ho);
  CLIST_FOR_EACH(struct main_hook *, ho, m->hook_list)
    hook_debug(ho);
  msg(L_DEBUG, "\tActive processes:");
  CLIST_FOR_EACH(struct main_process *, pr, m->process_list)
    process_debug(pr);
  msg(L_DEBUG, "\tActive signal catchers:");
  CLIST_FOR_EACH(struct main_signal *, sg, m->signal_list)
    signal_debug(sg);
}

#else

// Stubs
void file_debug(struct main_file *fi UNUSED) { }
void hook_debug(struct main_hook *ho UNUSED) { }
void signal_debug(struct main_signal *sg UNUSED) { }
void timer_debug(struct main_timer *tm UNUSED) { }
void process_debug(struct main_process *pr UNUSED) { }
void main_debug_context(struct main_context *m UNUSED) { }

#endif

static void
process_timers(struct main_context *m)
{
  struct main_timer *tm;
  while (count_timers(m) && (tm = m->timer_table[1])->expires <= m->now)
    {
      DBG("MAIN: Timer %p expired at now-%lld", tm, (long long)(m->now - tm->expires));
      tm->handler(tm);
    }
}

static enum main_hook_return
process_hooks(struct main_context *m)
{
  int hook_min = HOOK_RETRY;
  int hook_max = HOOK_SHUTDOWN;
  struct main_hook *ho;

  while (ho = clist_remove_head(&m->hook_list))
    {
      clist_add_tail(&m->hook_done_list, &ho->n);
      DBG("MAIN: Hook %p", ho);
      int ret = ho->handler(ho);
      hook_min = MIN(hook_min, ret);
      hook_max = MAX(hook_max, ret);
    }
  clist_move(&m->hook_list, &m->hook_done_list);
  if (hook_min == HOOK_SHUTDOWN ||
    hook_min == HOOK_DONE && hook_max == HOOK_DONE ||
    m->shutdown)
    {
      DBG("MAIN: Shut down by %s", m->shutdown ? "main_shut_down" : "a hook");
      return HOOK_SHUTDOWN;
    }
  if (hook_max == HOOK_RETRY)
    return HOOK_RETRY;
  else
    return HOOK_IDLE;
}

#ifdef CONFIG_UCW_EPOLL

static void
recalc_files(struct main_context *m)
{
  struct main_file *fi;

  while (fi = clist_remove_head(&m->file_recalc_list))
    {
      struct epoll_event evt = {
	.events = file_want_events(fi),
	.data.ptr = fi,
      };
      if (evt.events != fi->last_want_events)
	{
	  DBG("MAIN: Changing requested events for fd %d to %x", fi->fd, evt.events);
	  fi->last_want_events = evt.events;
	  if (epoll_ctl(main_current()->epoll_fd, EPOLL_CTL_MOD, fi->fd, &evt) < 0)
	    die("epoll_ctl() failed: %m");
	}
      clist_add_tail(&m->file_list, &fi->n);
    }
}

#else

static void
rebuild_poll_table(struct main_context *m)
{
  GARY_INIT_OR_RESIZE(m->poll_table, m->file_cnt);
  GARY_INIT_OR_RESIZE(m->poll_file_table, m->file_cnt);
  DBG("MAIN: Rebuilding poll table: %d entries", m->file_cnt);

  struct pollfd *p = m->poll_table;
  struct main_file **pf = m->poll_file_table;
  CLIST_FOR_EACH(struct main_file *, fi, m->file_list)
    {
      p->fd = fi->fd;
      p->events = file_want_events(fi);
      fi->pollfd = p++;
      *pf++ = fi;
    }
  m->poll_table_obsolete = 0;
}

#endif

void
main_loop(void)
{
  DBG("MAIN: Entering main_loop");
  struct main_context *m = main_current();

  main_get_time_ctx(m);
  m->shutdown = 0;

  for (;;)
    {
      timestamp_t wake = m->now + 1000000000;
      process_timers(m);
      switch (process_hooks(m))
	{
	case HOOK_SHUTDOWN:
	  return;
	case HOOK_RETRY:
	  wake = 0;
	  break;
	default: ;
	}

      int timeout = 0;
      if (!m->single_step)
	{
	  if (count_timers(m))
	    wake = MIN(wake, m->timer_table[1]->expires);
	  main_get_time_ctx(m);
	  timeout = ((wake > m->now) ? wake - m->now : 0);
	}

#ifdef CONFIG_UCW_EPOLL
      recalc_files(m);
      DBG("MAIN: Epoll for %d fds and timeout %d ms", m->file_cnt, timeout);
      int n = epoll_wait(m->epoll_fd, m->epoll_events, EPOLL_BUF_SIZE, timeout);
#else
      if (m->poll_table_obsolete)
	rebuild_poll_table(m);
      DBG("MAIN: Poll for %d fds and timeout %d ms", m->file_cnt, timeout);
      int n = poll(m->poll_table, m->file_cnt, timeout);
#endif

      DBG("\t-> %d events", n);
      if (n < 0 && errno != EAGAIN && errno != EINTR)
	die("(e)poll failed: %m");
      timestamp_t old_now = m->now;
      main_get_time_ctx(m);
      m->idle_time += m->now - old_now;

      if (n <= 0)
	{
	  if (m->single_step)
	    return;
	  else
	    continue;
	}

      // Relink all files with a pending event to file_active_list
#ifdef CONFIG_UCW_EPOLL
      for (int i=0; i<n; i++)
	{
	  struct epoll_event *e = &m->epoll_events[i];
	  struct main_file *fi = e->data.ptr;
	  clist_remove(&fi->n);
	  clist_add_tail(&m->file_active_list, &fi->n);
	  fi->events = e->events;
	}
#else
      struct pollfd *p = m->poll_table;
      struct main_file **pf = m->poll_file_table;
      for (uns i=0; i < m->file_cnt; i++)
	if (p[i].revents)
	  {
	    struct main_file *fi = pf[i];
	    clist_remove(&fi->n);
	    clist_add_tail(&m->file_active_list, &fi->n);
	    fi->events = p[i].revents;
	  }
#endif

      /*
       *  Process the buffered file events. This is pretty tricky, since
       *  user callbacks can modify the file structure or even destroy it.
       *  In such cases, we detect that the structure was relinked and stop
       *  processing its events, leaving them for the next iteration of the
       *  main loop.
       */
      struct main_file *fi;
      while (fi = clist_head(&m->file_active_list))
	{
	  if (fi->read_handler && (fi->events & (POLLIN | POLLHUP)))
	    {
	      fi->events &= ~(POLLIN | POLLHUP);
	      do
		DBG("MAIN: Read event on fd %d", fi->fd);
	      while (fi->read_handler && fi->read_handler(fi));
	      continue;
	    }
	  if (fi->write_handler && (fi->events & (POLLOUT | POLLHUP | POLLERR)))
	    {
	      fi->events &= ~(POLLOUT | POLLHUP | POLLERR);
	      do
		DBG("MAIN: Write event on fd %d", fi->fd);
	      while (fi->write_handler && fi->write_handler(fi));
	      continue;
	    }
	  clist_remove(&fi->n);
	  clist_add_tail(&m->file_list, &fi->n);
	}
    }
}

void
main_step(void)
{
  struct main_context *m = main_current();
  m->single_step = 1;
  main_loop();
  m->single_step = 0;
}
