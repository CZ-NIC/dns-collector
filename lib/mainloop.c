/*
 *	Sherlock Library -- Main Loop
 *
 *	(c) 2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mainloop.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/wait.h>

sh_time_t now;
uns main_shutdown;

static clist main_timer_list, main_file_list, main_hook_list, main_process_list;
static uns main_file_cnt;
static uns main_poll_table_obsolete, main_poll_table_size;
static struct pollfd *main_poll_table;
static uns main_sigchld_set_up;

void
main_init(void)
{
  DBG("MAIN: Initializing");
  clist_init(&main_timer_list);
  clist_init(&main_file_list);
  clist_init(&main_hook_list);
  clist_init(&main_process_list);
  now = time(NULL);
}

void
timer_add(struct main_timer *tm, sh_time_t expires)
{
  if (tm->expires)
    clist_remove(&tm->n);
  tm->expires = expires;
  if (expires)
    {
      cnode *t = main_timer_list.head.next;
      while (t != &main_timer_list.head && ((struct main_timer *) t)->expires < expires)
	t = t->next;
      clist_insert_before(&tm->n, t);
    }
}

void
timer_del(struct main_timer *tm)
{
  timer_add(tm, 0);
}

static void
file_timer_expired(struct main_timer *tm)
{
  struct main_file *fi = tm->data;
  timer_del(&fi->timer);
  if (fi->error_handler)
    fi->error_handler(fi, MFERR_TIMEOUT);
}

void
file_add(struct main_file *fi)
{
  ASSERT(!fi->n.next);
  clist_add_tail(&main_file_list, &fi->n);
  fi->timer.handler = file_timer_expired;
  fi->timer.data = fi;
  main_file_cnt++;
  main_poll_table_obsolete = 1;
  if (fcntl(fi->fd, F_SETFL, O_NONBLOCK) < 0)
    log(L_ERROR, "Error setting fd %d to non-blocking mode: %m. Keep fingers crossed.", fi->fd);
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
  ASSERT(fi->n.next);
  timer_del(&fi->timer);
  clist_remove(&fi->n);
  main_file_cnt--;
  main_poll_table_obsolete = 1;
  fi->n.next = fi->n.prev = NULL;
}

static int
file_read_handler(struct main_file *fi)
{
  while (fi->rpos < fi->rlen)
    {
      int l = read(fi->fd, fi->rbuf + fi->rpos, fi->rlen - fi->rpos);
      DBG("MAIN: FD %d: read %d", fi->fd, l);
      if (l < 0)
	{
	  if (errno != EINTR && errno != EAGAIN && fi->error_handler)
	    fi->error_handler(fi, MFERR_READ);
	  return 0;
	}
      else if (!l)
	break;
      fi->rpos += l;
    }
  DBG("MAIN: FD %d done read %d of %d", fi->fd, fi->rpos, fi->rlen);
  fi->read_handler = NULL;
  file_chg(fi);
  fi->read_done(fi);
  return 1;
}

static int
file_write_handler(struct main_file *fi)
{
  while (fi->wpos < fi->wlen)
    {
      int l = write(fi->fd, fi->wbuf + fi->wpos, fi->wlen - fi->wpos);
      DBG("MAIN: FD %d: write %d", fi->fd, l);
      if (l < 0)
	{
	  if (errno != EINTR && errno != EAGAIN && fi->error_handler)
	    fi->error_handler(fi, MFERR_WRITE);
	  return 0;
	}
      fi->wpos += l;
    }
  DBG("MAIN: FD %d done write %d", fi->fd, fi->wpos);
  fi->write_handler = NULL;
  file_chg(fi);
  fi->write_done(fi);
  return 1;
}

void
file_read(struct main_file *fi, byte *buf, uns len)
{
  ASSERT(fi->n.next);
  if (len)
    {
      fi->read_handler = file_read_handler;
      fi->rbuf = buf;
      fi->rpos = 0;
      fi->rlen = len;
    }
  else
    {
      fi->read_handler = NULL;
      fi->rbuf = NULL;
      fi->rpos = fi->rlen = 0;
    }
  file_chg(fi);
}

void
file_write(struct main_file *fi, byte *buf, uns len)
{
  ASSERT(fi->n.next);
  if (len)
    {
      fi->write_handler = file_write_handler;
      fi->wbuf = buf;
      fi->wpos = 0;
      fi->wlen = len;
    }
  else
    {
      fi->write_handler = NULL;
      fi->wbuf = NULL;
      fi->wpos = fi->wlen = 0;
    }
  file_chg(fi);
}

void
file_set_timeout(struct main_file *fi, sh_time_t expires)
{
  ASSERT(fi->n.next);
  timer_add(&fi->timer, expires);
}

void
hook_add(struct main_hook *ho)
{
  ASSERT(!ho->n.next);
  clist_add_tail(&main_hook_list, &ho->n);
}

void
hook_del(struct main_hook *ho)
{
  ASSERT(ho->n.next);
  clist_remove(&ho->n);
  ho->n.next = ho->n.prev = NULL;
}

static void
main_sigchld_handler(int x UNUSED)
{
  DBG("SIGCHLD received");
}

void
process_add(struct main_process *mp)
{
  ASSERT(!mp->n.next);
  clist_add_tail(&main_process_list, &mp->n);
  if (!main_sigchld_set_up)
    {
      struct sigaction sa;
      bzero(&sa, sizeof(sa));
      sa.sa_handler = main_sigchld_handler;
      sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
      sigaction(SIGCHLD, &sa, NULL);
      main_sigchld_set_up = 1;
    }
}

void
process_del(struct main_process *mp)
{
  ASSERT(mp->n.next);
  clist_remove(&mp->n);
  mp->pid = 0;
  mp->n.next = NULL;
}

int
process_fork(struct main_process *mp)
{
  pid_t pid = fork();
  if (pid < 0)
    return -1;
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

void
main_debug(void)
{
#ifdef DEBUG
  log(L_DEBUG, "### Main loop status on %d", (int)now);
  log(L_DEBUG, "\tActive timers:");
  struct main_timer *tm;
  CLIST_WALK(tm, main_timer_list)
    log(L_DEBUG, "\t\t%p (expires %d, data %p)", tm, (int)tm->expires, tm->data);
  struct main_file *fi;
  log(L_DEBUG, "\tActive files:");
  CLIST_WALK(fi, main_file_list)
    log(L_DEBUG, "\t\t%p (fd %d, rh %p, wh %p, eh %p, expires %d, data %p)",
	fi, fi->fd, fi->read_handler, fi->write_handler, fi->error_handler, fi->timer.expires, fi->data);
  log(L_DEBUG, "\tActive hooks:");
  struct main_hook *ho;
  CLIST_WALK(ho, main_hook_list)
    log(L_DEBUG, "\t\t%p (func %p, data %p)", ho, ho->handler, ho->data);
  log(L_DEBUG, "\tActive processes:");
  struct main_process *pr;
  CLIST_WALK(pr, main_process_list)
    log(L_DEBUG, "\t\t%p (pid %d, data %p)", pr, pr->pid, pr->data);
#endif
}

static void
main_rebuild_poll_table(void)
{
  struct main_file *fi;
  if (main_poll_table_size < main_file_cnt)
    {
      if (main_poll_table)
	xfree(main_poll_table);
      else
	main_poll_table_size = 1;
      while (main_poll_table_size < main_file_cnt)
	main_poll_table_size *= 2;
      main_poll_table = xmalloc(sizeof(struct pollfd) * main_poll_table_size);
    }
  struct pollfd *p = main_poll_table;
  DBG("MAIN: Rebuliding poll table: %d of %d entries set", main_file_cnt, main_poll_table_size);
  CLIST_WALK(fi, main_file_list)
    {
      p->fd = fi->fd;
      fi->pollfd = p++;
      file_chg(fi);
    }
  main_poll_table_obsolete = 0;
}

void
main_loop(void)
{
  DBG("MAIN: Entering main_loop");
  ASSERT(main_timer_list.head.next);

  struct main_file *fi;
  struct main_hook *ho;
  struct main_timer *tm;
  struct main_process *pr;
  cnode *tmp;

  while (!main_shutdown)
    {
      now = time(NULL);
      sh_time_t wake = now + 60;
      while ((tm = clist_head(&main_timer_list)) && tm->expires <= now)
	{
	  DBG("MAIN: Timer %p expired", tm);
	  tm->handler(tm);
	}
      CLIST_WALK_DELSAFE(ho, main_hook_list, tmp)
	{
	  DBG("MAIN: Hook %p", ho);
	  if (ho->handler(ho))
	    wake = 0;
	}
      if (main_poll_table_obsolete)
	main_rebuild_poll_table();
      if (!clist_empty(&main_process_list))
	{
	  int stat;
	  pid_t pid;
	  while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
	    {
	      DBG("MAIN: Child %d exited with status %x", pid, stat);
	      CLIST_WALK(pr, main_process_list)
		if (pr->pid == pid)
		  {
		    pr->status = stat;
		    process_del(pr);
		    DBG("MAIN: Calling process exit handler");
		    pr->handler(pr);
		    break;
		  }
	      wake = 0;
	    }
	}
      /* FIXME: Here is a small race window where SIGCHLD can come unnoticed. */
      if ((tm = clist_head(&main_timer_list)) && tm->expires < wake)
	wake = tm->expires;
      int timeout = (wake ? (wake - now) * 1000 : 0);
      DBG("MAIN: Poll for %d fds and timeout %d ms", main_file_cnt, timeout);
      if (poll(main_poll_table, main_file_cnt, timeout))
	{
	  struct pollfd *p = main_poll_table;
	  now = time(NULL);
	  CLIST_WALK(fi, main_file_list)
	    {
	      if (p->revents & (POLLIN | POLLHUP | POLLERR))
		{
		  do
		    DBG("MAIN: Read event on fd %d", p->fd);
		  while (fi->read_handler && fi->read_handler(fi) && !main_poll_table_obsolete);
		  if (main_poll_table_obsolete)	/* File entries have been inserted or deleted => better not risk continuing to nowhere */
		    break;
		}
	      if (p->revents & (POLLOUT | POLLERR))
		{
		  do
		    DBG("MAIN: Write event on fd %d", p->fd);
		  while (fi->write_handler && fi->write_handler(fi) && !main_poll_table_obsolete);
		  if (main_poll_table_obsolete)
		    break;
		}
	      p++;
	    }
	}
    }
}

#ifdef TEST

static struct main_process mp;
static struct main_file fin, fout;
static struct main_hook hook;
static struct main_timer tm;

static byte rb[16];

static void dread(struct main_file *fi)
{
  if (fi->rpos < fi->rlen)
    {
      log(L_INFO, "Read EOF");
      file_del(fi);
    }
  else
    {
      log(L_INFO, "Read done");
      file_read(fi, rb, sizeof(rb));
    }
}

static void derror(struct main_file *fi, int cause)
{
  log(L_INFO, "Error: %m !!! (cause %d)", cause);
  file_del(fi);
}

static void dwrite(struct main_file *fi UNUSED)
{
  log(L_INFO, "Write done");
}

static int dhook(struct main_hook *ho UNUSED)
{
  log(L_INFO, "Hook called");
  return 0;
}

static void dtimer(struct main_timer *tm)
{
  log(L_INFO, "Timer tick");
  timer_add(tm, now + 10);
}

static void dentry(struct main_process *pr UNUSED)
{
  log(L_INFO, "*** SUBPROCESS START ***");
  sleep(2);
  log(L_INFO, "*** SUBPROCESS FINISH ***");
}

static void dexit(struct main_process *pr)
{
  log(L_INFO, "Subprocess %d exited with status %x", pr->pid, pr->status);
}

int
main(void)
{
  log_init(NULL);
  main_init();

  fin.fd = 0;
  fin.read_done = dread;
  fin.error_handler = derror;
  file_add(&fin);
  file_read(&fin, rb, sizeof(rb));

  fout.fd = 1;
  fout.write_done = dwrite;
  fout.error_handler = derror;
  file_add(&fout);
  file_write(&fout, "Hello, world!\n", 14);

  hook.handler = dhook;
  hook_add(&hook);

  tm.handler = dtimer;
  timer_add(&tm, now + 1);

  mp.handler = dexit;
  if (!process_fork(&mp))
    dentry();

  main_debug();

  main_loop();
  log(L_INFO, "Finished.");
}

#endif
