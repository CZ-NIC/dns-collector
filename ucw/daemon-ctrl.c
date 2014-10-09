/*
 *	UCW Library -- Daemon Control
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *	(c) 2014 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/daemon.h>
#include <ucw/process.h>
#include <ucw/strtonum.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>

static enum daemon_control_status
daemon_control_err(struct daemon_control_params *dc, char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  vsnprintf(dc->error_msg, DAEMON_ERR_LEN, msg, args);
  va_end(args);
  return DAEMON_STATUS_ERROR;
}

static enum daemon_control_status
daemon_read_pid(struct daemon_control_params *dc, int will_wait, int *pidp)
{
  // Possible results:
  // -- DAEMON_STATUS_ERROR, pid == 0
  // -- DAEMON_STATUS_NOT_RUNNING, pid == 0
  // -- DAEMON_STATUS_OK, pid > 0
  // -- DAEMON_STATUS_OK, pid == 0 (flocked, but no pid available during start or stop)

  enum daemon_control_status st = DAEMON_STATUS_NOT_RUNNING;
  *pidp = 0;

  int pid_fd = open(dc->pid_file, O_RDONLY);
  if (pid_fd < 0)
    {
      if (errno == ENOENT)
	return st;
      return daemon_control_err(dc, "Cannot open PID file `%s': %m", dc->pid_file);
    }

  while (flock(pid_fd, LOCK_SH | (will_wait ? 0 : LOCK_NB)) < 0)
    {
      if (errno == EWOULDBLOCK)
	{
	  st = DAEMON_STATUS_OK;
	  break;
	}
      else if (errno != EINTR)
	{
	  daemon_control_err(dc, "Cannot lock PID file `%s': %m", dc->pid_file);
	  goto fail;
	}
    }

  char buf[16];
  int n = read(pid_fd, buf, sizeof(buf));
  if (n < 0)
    {
      daemon_control_err(dc, "Error reading `%s': %m", dc->pid_file);
      goto fail;
    }
  if (n == (int) sizeof(buf))
    {
      daemon_control_err(dc, "PID file `%s' is too long", dc->pid_file);
      goto fail;
    }
  buf[n] = 0;

  if (!n)
    {
      close(pid_fd);
      return st;
    }

  if (st != DAEMON_STATUS_OK)
    {
      close(pid_fd);
      return DAEMON_STATUS_STALE;
    }

  int pid;
  const char *next;
  if (str_to_int(&pid, buf, &next, 10) || *next != '\n')
    {
      daemon_control_err(dc, "PID file `%s' does not contain a valid PID", dc->pid_file);
      goto fail;
    }
  close(pid_fd);
  *pidp = pid;
  return DAEMON_STATUS_OK;

fail:
  close(pid_fd);
  return DAEMON_STATUS_ERROR;
}

enum daemon_control_status
daemon_control(struct daemon_control_params *dc)
{
  int guard_fd = open(dc->guard_file, O_RDWR | O_CREAT, 0666);
  if (guard_fd < 0)
    return daemon_control_err(dc, "Cannot open guard file `%s': %m", dc->guard_file);
  if (flock(guard_fd, LOCK_EX) < 0)
    return daemon_control_err(dc, "Cannot lock guard file `%s': %m", dc->guard_file);

  // Read the PID file
  int pid, sig;
  enum daemon_control_status st = daemon_read_pid(dc, 0, &pid);
  if (st == DAEMON_STATUS_ERROR)
    goto done;

  switch (dc->action)
    {
    case DAEMON_CONTROL_CHECK:
      break;
    case DAEMON_CONTROL_START:
      if (st == DAEMON_STATUS_OK)
	st = DAEMON_STATUS_ALREADY_DONE;
      else
	{
	  pid_t pp = fork();
	  if (pp < 0)
	    {
	      st = daemon_control_err(dc, "Cannot fork: %m");
	      goto done;
	    }
	  if (!pp)
	    {
	      close(guard_fd);
	      execvp(dc->argv[0], dc->argv);
	      fprintf(stderr, "Cannot execute `%s': %m\n", dc->argv[0]);
	      exit(DAEMON_STATUS_ERROR);
	    }
	  int stat;
	  int ec = waitpid(pp, &stat, 0);
	  if (ec < 0)
	    {
	      st = daemon_control_err(dc, "Cannot wait: %m");
	      goto done;
	    }
	  if (WIFEXITED(stat) && WEXITSTATUS(stat) == DAEMON_STATUS_ERROR)
	    {
	      st = daemon_control_err(dc, "Cannot execute the daemon");
	      goto done;
	    }
	  char ecmsg[EXIT_STATUS_MSG_SIZE];
	  if (format_exit_status(ecmsg, stat))
	    {
	      st = daemon_control_err(dc, "Daemon %s %s", dc->argv[0], ecmsg);
	      goto done;
	    }
	  if (st != DAEMON_STATUS_STALE)
	    st = DAEMON_STATUS_OK;
	}
      break;
    case DAEMON_CONTROL_STOP:
      if (st != DAEMON_STATUS_OK)
	{
	  if (st == DAEMON_STATUS_NOT_RUNNING)
	    st = DAEMON_STATUS_ALREADY_DONE;
	  goto done;
	}
      if (pid)
	{
	  sig = dc->signal ? : SIGTERM;
	  if (kill(pid, sig) < 0)
	    {
	      st = daemon_control_err(dc, "Cannot send signal %d: %m", sig);
	      goto done;
	    }
	}
      else
	{
	  // With locked guard file it's sure that daemon is currently exiting
	  // and not starting => we can safely wait without any signal
	}
      st = daemon_read_pid(dc, 1, &pid);
      if (st != DAEMON_STATUS_ERROR)
	st = DAEMON_STATUS_OK;
      break;
    case DAEMON_CONTROL_SIGNAL:
      if (!pid)
	return DAEMON_STATUS_NOT_RUNNING;
      sig = dc->signal ? : SIGHUP;
      if (kill(pid, sig) < 0)
	st = daemon_control_err(dc, "Cannot send signal %d: %m", sig);
      else
	st = DAEMON_STATUS_OK;
      break;
    default:
      ASSERT(0);
    }

done:
  close(guard_fd);
  return st;
}
