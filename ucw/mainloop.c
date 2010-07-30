/*
 *	UCW Library -- Main Loop
 *
 *	(c) 2004--2010 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "ucw/heap.h"
#include "ucw/mainloop.h"
#include "ucw/threads.h"
#include "ucw/gary.h"

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

#define MAIN_TIMER_LESS(x,y) ((x)->expires < (y)->expires)
#define MAIN_TIMER_SWAP(heap,a,b,t) (t=heap[a], heap[a]=heap[b], heap[b]=t, heap[a]->index=(a), heap[b]->index=(b))

static void
do_main_get_time(struct main_context *m)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  m->now_seconds = tv.tv_sec;
  m->now = (timestamp_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

struct main_context *
main_new(void)
{
  struct main_context *m = xmalloc_zero(sizeof(*m));

  DBG("MAIN: New context");
  clist_init(&m->file_list);
  clist_init(&m->hook_list);
  clist_init(&m->hook_done_list);
  clist_init(&m->process_list);
  clist_init(&m->signal_list);
  m->poll_table_obsolete = 1;
  do_main_get_time(m);
  sigemptyset(&m->want_signals);
  m->sig_pipe_recv = m->sig_pipe_send = -1;

  return m;
}

void
main_delete(struct main_context *m)
{
  if (m->sigchld_handler)
    signal_del(m->sigchld_handler);
  if (m->sig_pipe_file)
    file_del(m->sig_pipe_file);
  if (m->sig_pipe_recv >= 0)
    {
      close(m->sig_pipe_recv);
      close(m->sig_pipe_send);
    }
  ASSERT(clist_empty(&m->file_list));
  ASSERT(clist_empty(&m->hook_list));
  ASSERT(clist_empty(&m->hook_done_list));
  ASSERT(clist_empty(&m->process_list));
  ASSERT(clist_empty(&m->signal_list));
  if (m->timer_table)
    GARY_FREE(m->timer_table);
  xfree(m->poll_table);
  xfree(m);
  // FIXME: Some mechanism for cleaning up after fork()
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

struct main_context *
main_current(void)
{
  struct ucwlib_context *c = ucwlib_thread_context();
  struct main_context *m = c->main_context;
  ASSERT(m);
  return m;
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
  struct main_context *m = main_switch_context(NULL);
  main_delete(m);
}

void
main_get_time(void)
{
  do_main_get_time(main_current());
}

static inline uns
count_timers(struct main_context *m)
{
  return GARY_SIZE(m->timer_table) - 1;
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
	  *GARY_PUSH(m->timer_table, 1) = tm;
	  HEAP_INSERT(struct main_timer *, m->timer_table, tm->index, MAIN_TIMER_LESS, MAIN_TIMER_SWAP);
	}
      else
	{
	  tm->expires = expires;
	  HEAP_INCREASE(struct main_timer *, m->timer_table, num_timers, MAIN_TIMER_LESS, MAIN_TIMER_SWAP, tm->index);
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
	  GARY_POP(m->timer_table, 1);
	}
      else
	{
	  tm->expires = expires;
	  HEAP_DECREASE(struct main_timer *, m->timer_table, num_timers, MAIN_TIMER_LESS, MAIN_TIMER_SWAP, tm->index);
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

void
file_add(struct main_file *fi)
{
  struct main_context *m = main_current();

  DBG("MAIN: Adding file %p (fd=%d)", fi, fi->fd);
  ASSERT(!clist_is_linked(&fi->n));
  clist_add_tail(&m->file_list, &fi->n);
  m->file_cnt++;
  m->poll_table_obsolete = 1;
  if (fcntl(fi->fd, F_SETFL, O_NONBLOCK) < 0)
    msg(L_ERROR, "Error setting fd %d to non-blocking mode: %m. Keep fingers crossed.", fi->fd);
}

void
file_chg(struct main_file *fi)
{
  struct pollfd *p = fi->pollfd;
  if (p)
    {
      p->events = 0;
      if (fi->read_handler)
	p->events |= POLLIN | POLLHUP | POLLERR;
      if (fi->write_handler)
	p->events |= POLLOUT | POLLERR;
    }
}

void
file_del(struct main_file *fi)
{
  struct main_context *m = main_current();

  DBG("MAIN: Deleting file %p (fd=%d)", fi, fi->fd);
  ASSERT(clist_is_linked(&fi->n));
  clist_unlink(&fi->n);
  m->file_cnt--;
  m->poll_table_obsolete = 1;
}

void
file_close_all(void)
{
  struct main_context *m = main_current();

  CLIST_FOR_EACH(struct main_file *, f, m->file_list)
    close(f->fd);
}

void
hook_add(struct main_hook *ho)
{
  struct main_context *m = main_current();

  DBG("MAIN: Adding hook %p", ho);
  ASSERT(!clist_is_linked(&ho->n));
  clist_add_tail(&m->hook_list, &ho->n);
}

void
hook_del(struct main_hook *ho)
{
  DBG("MAIN: Deleting hook %p", ho);
  ASSERT(clist_is_linked(&ho->n));
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
  ASSERT(!clist_is_linked(&mp->n));
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
  ASSERT(clist_is_linked(&mp->n));
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
  struct main_signal *tmp;
  CLIST_FOR_EACH_DELSAFE(struct main_signal *, sg, m->signal_list, tmp)
    if (sg->signum == signum)
      {
	DBG("MAIN: Sigpipe: invoking handler %p", sg);
	// FIXME: Can the handler disappear from here?
	sg->handler(sg);
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

  ASSERT(!clist_is_linked(&ms->n));
  clist_add_tail(&m->signal_list, &ms->n);
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

void
signal_del(struct main_signal *ms)
{
  struct main_context *m = main_current();

  DBG("MAIN: Deleting signal %p (sig=%d)", ms, ms->signum);

  ASSERT(clist_is_linked(&ms->n));
  clist_unlink(&ms->n);

  int another = 0;
  CLIST_FOR_EACH(struct main_signal *, s, m->signal_list)
    if (s->signum == ms->signum)
      another++;
  if (!another)
    {
      sigset_t ss;
      sigemptyset(&ss);
      sigaddset(&ss, ms->signum);
      THREAD_SIGMASK(SIG_BLOCK, &ss, NULL);
      sigdelset(&m->want_signals, ms->signum);
    }
}

void
main_debug_context(struct main_context *m UNUSED)
{
#ifdef CONFIG_DEBUG
  msg(L_DEBUG, "### Main loop status on %lld", (long long) m->now);
  msg(L_DEBUG, "\tActive timers:");
  uns num_timers = count_timers(m);
  for (uns i = 1; i <= num_timers; i++)
    {
      struct main_timer *tm = m->timer_table[i];
      msg(L_DEBUG, "\t\t%p (expires %lld, data %p)", tm, (long long)(tm->expires ? tm->expires - m->now : 999999), tm->data);
    }
  msg(L_DEBUG, "\tActive files:");
  CLIST_FOR_EACH(struct main_file *, fi, m->file_list)
    msg(L_DEBUG, "\t\t%p (fd %d, rh %p, wh %p, data %p)",
	fi, fi->fd, fi->read_handler, fi->write_handler, fi->data);
    // FIXME: Can we display status of block_io requests somehow?
  msg(L_DEBUG, "\tActive hooks:");
  CLIST_FOR_EACH(struct main_hook *, ho, m->hook_done_list)
    msg(L_DEBUG, "\t\t%p (func %p, data %p)", ho, ho->handler, ho->data);
  CLIST_FOR_EACH(struct main_hook *, ho, m->hook_list)
    msg(L_DEBUG, "\t\t%p (func %p, data %p)", ho, ho->handler, ho->data);
  msg(L_DEBUG, "\tActive processes:");
  CLIST_FOR_EACH(struct main_process *, pr, m->process_list)
    msg(L_DEBUG, "\t\t%p (pid %d, func %p, data %p)", pr, pr->pid, pr->handler, pr->data);
  msg(L_DEBUG, "\tActive signal catchers:");
  CLIST_FOR_EACH(struct main_signal *, sg, m->signal_list)
    msg(L_DEBUG, "\t\t%p (sig %d, func %p, data %p)", sg, sg->signum, sg->handler, sg->data);
#endif
}

static void
main_rebuild_poll_table(struct main_context *m)
{
  struct main_file *fi;
  if (m->poll_table_size < m->file_cnt)
    {
      if (m->poll_table)
	xfree(m->poll_table);
      else
	m->poll_table_size = 1;
      while (m->poll_table_size < m->file_cnt)
	m->poll_table_size *= 2;
      m->poll_table = xmalloc(sizeof(struct pollfd) * m->poll_table_size);
    }
  struct pollfd *p = m->poll_table;
  DBG("MAIN: Rebuilding poll table: %d of %d entries set", m->file_cnt, m->poll_table_size);
  CLIST_WALK(fi, m->file_list)
    {
      p->fd = fi->fd;
      fi->pollfd = p++;
      file_chg(fi);
    }
  m->poll_table_obsolete = 0;
}

void
main_loop(void)
{
  DBG("MAIN: Entering main_loop");
  struct main_context *m = main_current();

  struct main_file *fi;
  struct main_hook *ho;
  struct main_timer *tm;

  do_main_get_time(m);
  for (;;)
    {
      timestamp_t wake = m->now + 1000000000;
      while (GARY_SIZE(m->timer_table) > 1 && (tm = m->timer_table[1])->expires <= m->now)
	{
	  DBG("MAIN: Timer %p expired at now-%lld", tm, (long long)(m->now - tm->expires));
	  tm->handler(tm);
	}
      int hook_min = HOOK_RETRY;
      int hook_max = HOOK_SHUTDOWN;
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
	  DBG("MAIN: Shut down by %s", m->shutdown ? "main_shutdown" : "a hook");
	  return;
	}
      if (hook_max == HOOK_RETRY)
	wake = 0;
      if (m->poll_table_obsolete)
	main_rebuild_poll_table(m);
      if (count_timers(m) && (tm = m->timer_table[1])->expires < wake)
	wake = tm->expires;
      do_main_get_time(m);
      int timeout = ((wake > m->now) ? wake - m->now : 0);
      DBG("MAIN: Poll for %d fds and timeout %d ms", m->file_cnt, timeout);
      int p = poll(m->poll_table, m->file_cnt, timeout);
      timestamp_t old_now = m->now;
      do_main_get_time(m);
      m->idle_time += m->now - old_now;
      if (p > 0)
	{
	  struct pollfd *p = m->poll_table;
	  CLIST_WALK(fi, m->file_list)
	    {
	      if (p->revents & (POLLIN | POLLHUP | POLLERR))
		{
		  do
		    DBG("MAIN: Read event on fd %d", p->fd);
		  while (fi->read_handler && fi->read_handler(fi) && !m->poll_table_obsolete);
		  if (m->poll_table_obsolete)	/* File entries have been inserted or deleted => better not risk continuing to nowhere */
		    break;
		}
	      if (p->revents & (POLLOUT | POLLERR))
		{
		  do
		    DBG("MAIN: Write event on fd %d", p->fd);
		  while (fi->write_handler && fi->write_handler(fi) && !m->poll_table_obsolete);
		  if (m->poll_table_obsolete)
		    break;
		}
	      p++;
	    }
	}
    }
}

#ifdef TEST

static struct main_process mp;
static struct main_block_io fin, fout;
static struct main_hook hook;
static struct main_timer tm;
static struct main_signal sg;

static byte rb[16];

static void dread(struct main_block_io *bio)
{
  if (bio->rpos < bio->rlen)
    {
      msg(L_INFO, "Read EOF");
      block_io_del(bio);
    }
  else
    {
      msg(L_INFO, "Read done");
      block_io_read(bio, rb, sizeof(rb));
    }
}

static void derror(struct main_block_io *bio, int cause)
{
  msg(L_INFO, "Error: %m !!! (cause %d)", cause);
  block_io_del(bio);
}

static void dwrite(struct main_block_io *bio UNUSED)
{
  msg(L_INFO, "Write done");
}

static int dhook(struct main_hook *ho UNUSED)
{
  msg(L_INFO, "Hook called");
  return 0;
}

static void dtimer(struct main_timer *tm)
{
  msg(L_INFO, "Timer tick");
  timer_add_rel(tm, 11000);
  timer_add_rel(tm, 10000);
}

static void dentry(void)
{
  msg(L_INFO, "*** SUBPROCESS START ***");
  sleep(2);
  msg(L_INFO, "*** SUBPROCESS FINISH ***");
  exit(0);
}

static void dexit(struct main_process *pr)
{
  msg(L_INFO, "Subprocess %d exited with status %x", pr->pid, pr->status);
}

static void dsignal(struct main_signal *sg UNUSED)
{
  msg(L_INFO, "SIGINT received (use Ctrl-\\ to really quit)");
}

int
main(void)
{
  log_init(NULL);
  main_init();

  fin.read_done = dread;
  fin.error_handler = derror;
  block_io_add(&fin, 0);
  block_io_read(&fin, rb, sizeof(rb));

  fout.write_done = dwrite;
  fout.error_handler = derror;
  block_io_add(&fout, 1);
  block_io_write(&fout, "Hello, world!\n", 14);

  hook.handler = dhook;
  hook_add(&hook);

  tm.handler = dtimer;
  timer_add_rel(&tm,  1000);

  sg.signum = SIGINT;
  sg.handler = dsignal;
  signal_add(&sg);

  mp.handler = dexit;
  if (!process_fork(&mp))
    dentry();

  main_debug();

  main_loop();
  msg(L_INFO, "Finished.");
}

#endif
