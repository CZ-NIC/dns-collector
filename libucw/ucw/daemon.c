/*
 *	UCW Library -- Daemonization
 *
 *	(c) 2012--2014 Martin Mares <mj@ucw.cz>
 *	(c) 2014 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/daemon.h>
#include <ucw/strtonum.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

void
daemon_resolve_ugid(struct daemon_params *dp)
{
  // Resolve user name
  const char *u = dp->run_as_user;
  struct passwd *pw = NULL;
  if (u)
    {
      if (u[0] == '#')
	{
	  uint id;
	  const char *err = str_to_uint(&id, u, NULL, 10 | STN_WHOLE);
	  if (err)
	    die("Cannot parse user `%s': %s", u, err);
	  dp->run_as_uid = id;
	  dp->want_setuid = 1;
	}
      else
	{
	  pw = getpwnam(u);
	  if (!pw)
	    die("No such user `%s'", u);
	  dp->run_as_uid = pw->pw_uid;
	  dp->want_setuid = 1;
	}
    }

  // Resolve group name
  const char *g = dp->run_as_group;
  struct group *gr;
  if (g)
    {
      if (g[0] == '#')
	{
	  uint id;
	  const char *err = str_to_uint(&id, g, NULL, 10 | STN_WHOLE);
	  if (err)
	    die("Cannot parse group `%s': %s", g, err);
	  dp->run_as_gid = id;
	  dp->want_setgid = 1;
	}
      else
	{
	  gr = getgrnam(g);
	  if (!gr)
	    die("No such group `%s'", g);
	  dp->run_as_gid = gr->gr_gid;
	  dp->want_setgid = 1;
	}
    }
  else if (pw)
    {
      dp->run_as_gid = pw->pw_gid;
      dp->want_setgid = 2;
    }
}

void
daemon_switch_ugid(struct daemon_params *dp)
{
  if (dp->want_setgid && setresgid(dp->run_as_gid, dp->run_as_gid, dp->run_as_gid) < 0)
    die("Cannot set GID to %d: %m", (int) dp->run_as_gid);
  if (dp->want_setgid > 1 && initgroups(dp->run_as_user, dp->run_as_gid) < 0)
    die("Cannot initialize groups: %m");
  if (dp->want_setuid && setresuid(dp->run_as_uid, dp->run_as_uid, dp->run_as_uid) < 0)
    die("Cannot set UID to %d: %m", (int) dp->run_as_uid);
}

void
daemon_init(struct daemon_params *dp)
{
  daemon_resolve_ugid(dp);

  if (dp->flags & DAEMON_FLAG_SIMULATE)
    return;

  if (dp->pid_file)
    {
      // Check that PID file path is absolute
      if (!(dp->flags & DAEMON_FLAG_PRESERVE_CWD) && dp->pid_file[0] != '/')
	die("Path to PID file `%s' must be absolute", dp->pid_file);

      // Open PID file
      dp->pid_fd = open(dp->pid_file, O_RDWR | O_CREAT, 0666);
      if (dp->pid_fd < 0)
	die("Cannot open `%s': %m", dp->pid_file);
      int fl = fcntl(dp->pid_fd, F_GETFD);
      if (fl < 0 || fcntl(dp->pid_fd, F_SETFD, fl | FD_CLOEXEC))
	die("Cannot set FD_CLOEXEC: %m");

      // Try to lock it with an exclusive lock
      if (flock(dp->pid_fd, LOCK_EX | LOCK_NB) < 0)
	{
	  if (errno == EINTR || errno == EWOULDBLOCK)
	    die("Daemon is already running (`%s' locked)", dp->pid_file);
	  else
	    die("Cannot lock `%s': %m", dp->pid_file);
	}

      // Make a note that the daemon is starting
      if (ftruncate(dp->pid_fd, 0) < 0 ||
          write(dp->pid_fd, "(starting)\n", 11) != 11)
	die("Error writing `%s': %m", dp->pid_file);
    }
}

void
daemon_run(struct daemon_params *dp, void (*body)(struct daemon_params *dp))
{
  if (dp->flags & DAEMON_FLAG_SIMULATE)
    {
      body(dp);
      return;
    }

  // Switch GID and UID
  daemon_switch_ugid(dp);

  // Create a new session and close stdio
  setsid();
  close(0);
  if (open("/dev/null", O_RDWR, 0) < 0 ||
      dup2(0, 1) < 0)
    die("Cannot redirect stdio to `/dev/null': %m");

  // Set umask to a reasonable value
  umask(022);

  // Do not hold the current working directory
  if (!(dp->flags & DAEMON_FLAG_PRESERVE_CWD))
    {
      if (chdir("/") < 0)
	die("Cannot chdir to root: %m");
    }

  // Create pipe to synchronize child process with master and avoid possible
  // collision between writing of PID and daemon_exit()
  int pipe_fd[2];
  if (dp->pid_file && pipe(pipe_fd) < 0)
    die("Cannot create pipe: %m");

  // Fork
  pid_t pid = fork();
  if (pid < 0)
    die("Cannot fork: %m");
  if (!pid)
    {
      // Wait for master process to finish writing of PID
      if (dp->pid_file)
	{
	  byte pipe_buf[1];
	  close(pipe_fd[1]);
	  if (read(pipe_fd[0], pipe_buf, 1) < 0)
	    die("Cannot read pipe: %m");
	  close(pipe_fd[0]);
	}

      // We still keep the PID file open and thus locked
      body(dp);
      exit(0);
    }

  // Write PID
  if (dp->pid_file)
    {
      char buf[32];
      int c = snprintf(buf, sizeof(buf), "%d\n", (int) pid);
      ASSERT(c <= (int) sizeof(buf));
      if (lseek(dp->pid_fd, 0, SEEK_SET) < 0 ||
	  write(dp->pid_fd, buf, c) != c ||
	  ftruncate(dp->pid_fd, c) ||
	  close(dp->pid_fd) < 0)
	die("Cannot write PID to `%s': %m", dp->pid_file);
      close(pipe_fd[0]);
      close(pipe_fd[1]);
    }
}

void
daemon_exit(struct daemon_params *dp)
{
  if (dp->flags & DAEMON_FLAG_SIMULATE)
    return;

  if (dp->pid_file)
    {
      if (ftruncate(dp->pid_fd, 0))
	die("Error truncating `%s': %m", dp->pid_file);
      close(dp->pid_fd);
    }
}

#ifdef TEST

#include <signal.h>

static volatile sig_atomic_t terminate;

static void term_handler(int sig UNUSED)
{
  msg(L_INFO | L_SIGHANDLER, "SIGTERM received, terminating in a while");
  terminate = 1;
}

static void hup_handler(int sig UNUSED)
{
  msg(L_INFO | L_SIGHANDLER, "SIGHUP received");
}

static void body(struct daemon_params *dp)
{
  log_fork();
  msg(L_INFO, "Daemon is running");
  msg(L_INFO, "uid=%d/%d gid=%d/%d", (int) getuid(), (int) geteuid(), (int) getgid(), (int) getegid());

  struct sigaction sa_term = { .sa_handler = term_handler };
  struct sigaction sa_hup = { .sa_handler = hup_handler };
  if (sigaction(SIGTERM, &sa_term, NULL) < 0 ||
      sigaction(SIGHUP, &sa_hup, NULL) < 0)
    ASSERT(0);

  while (!terminate)
    {
      if (!sleep(60))
	{
	  msg(L_INFO, "Timeout elapsed, terminating in a while");
	  break;
	}
    }

  sleep(2);
  msg(L_INFO, "Daemon is shutting down");
  daemon_exit(dp);
}

int main(int argc, char **argv)
{
  struct daemon_params dp = {
    .pid_file = "/tmp/123",
  };

  int opt;
  while ((opt = getopt(argc, argv, "p:u:g:")) >= 0)
    switch (opt)
      {
      case 'p':
	dp.pid_file = optarg;
	break;
      case 'u':
	dp.run_as_user = optarg;
	break;
      case 'g':
	dp.run_as_group = optarg;
	break;
      default:
	die("Invalid arguments");
      }

  daemon_init(&dp);
  daemon_run(&dp, body);
  msg(L_INFO, "Main program has ended");
  return 0;
}

#endif
