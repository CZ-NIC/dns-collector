/*
 *	UCW Library -- Daemon Control
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/daemon.h>
#include <ucw/process.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
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

static int
daemon_read_pid(struct daemon_control_params *dc, int will_wait)
{
  int pid_fd = open(dc->pid_file, O_RDONLY);
  if (pid_fd < 0)
    {
      if (errno == ENOENT)
	return 0;
      daemon_control_err(dc, "Cannot open PID file `%s': %m", dc->pid_file);
      return -1;
    }

  if (flock(pid_fd, LOCK_EX | (will_wait ? 0 : LOCK_NB)) >= 0)
    {
      // The lock file is stale
      close(pid_fd);
      return 0;
    }

  if (errno != EINTR && errno != EWOULDBLOCK)
    {
      daemon_control_err(dc, "Cannot lock PID file `%s': %m", dc->pid_file);
      goto fail;
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
  int pid = atoi(buf);
  if (!pid)
    {
      daemon_control_err(dc, "PID file `%s' does not contain a valid PID", dc->pid_file);
      goto fail;
    }
  close(pid_fd);
  return pid;

fail:
  close(pid_fd);
  return -1;
}

enum daemon_control_status
daemon_control(struct daemon_control_params *dc)
{
  enum daemon_control_status st = DAEMON_STATUS_ERROR;
  int sig;

  int guard_fd = open(dc->guard_file, O_RDWR | O_CREAT, 0666);
  if (guard_fd < 0)
    return daemon_control_err(dc, "Cannot open guard file `%s': %m", dc->guard_file);
  if (flock(guard_fd, LOCK_EX) < 0)
    return daemon_control_err(dc, "Cannot lock guard file `%s': %m", dc->guard_file);

  // Read the PID file
  int pid = daemon_read_pid(dc, 0);
  if (pid < 0)
    goto done;

  switch (dc->action)
    {
    case DAEMON_CONTROL_CHECK:
      if (pid)
	st = DAEMON_STATUS_OK;
      else
	st = DAEMON_STATUS_NOT_RUNNING;
      break;
    case DAEMON_CONTROL_START:
      if (pid)
	st = DAEMON_STATUS_ALREADY_DONE;
      else
	{
	  pid_t pp = fork();
	  if (pp < 0)
	    {
	      daemon_control_err(dc, "Cannot fork: %m");
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
	      daemon_control_err(dc, "Cannot wait: %m");
	      goto done;
	    }
	  if (WIFEXITED(stat) && WEXITSTATUS(stat) == DAEMON_STATUS_ERROR)
	    {
	      daemon_control_err(dc, "Cannot execute the daemon");
	      goto done;
	    }
	  char ecmsg[EXIT_STATUS_MSG_SIZE];
	  if (format_exit_status(ecmsg, stat))
	    {
	      daemon_control_err(dc, "Daemon %s %s", dc->argv[0], ecmsg);
	      goto done;
	    }
	  pid = daemon_read_pid(dc, 0);
	  if (!pid)
	    daemon_control_err(dc, "Daemon %s failed to write the PID file `%s'", dc->argv[0], dc->pid_file);
	  else
	    st = DAEMON_STATUS_OK;
	}
      break;
    case DAEMON_CONTROL_STOP:
      if (!pid)
	return DAEMON_STATUS_ALREADY_DONE;
      sig = dc->signal ? : SIGTERM;
      if (kill(pid, sig) < 0)
	{
	  daemon_control_err(dc, "Cannot send signal %d: %m", sig);
	  goto done;
	}
      pid = daemon_read_pid(dc, 1);
      ASSERT(pid <= 0);
      if (!pid)
	st = DAEMON_STATUS_OK;
      break;
    case DAEMON_CONTROL_SIGNAL:
      if (!pid)
	return DAEMON_STATUS_NOT_RUNNING;
      sig = dc->signal ? : SIGHUP;
      if (kill(pid, sig) < 0)
	daemon_control_err(dc, "Cannot send signal %d: %m", sig);
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
