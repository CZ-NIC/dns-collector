/*
 *	UCW Library Utilities -- A Simple Logger for use in shell scripts
 *
 *	(c) 2001--2009 Martin Mares <mj@ucw.cz>
 *      (c) 2011 Tomas Ebenlendr <ebik@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/log.h>
#include <ucw/mainloop.h>
#include <ucw/clists.h>
#include <ucw/getopt.h>
#include <ucw/conf.h>
#include <ucw/process.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

static uns max_line = 1024;
static int launch_finish_messages = 1;
static int nonzero_status_message = 1;

static struct cf_section cfsec_logoutput = {
  CF_ITEMS {
    CF_UNS("LineMax", &max_line),
    CF_END
  }
};

static clist filedescriptors;

struct fds {
  cnode node;
  int pipe[2];
  int fdnum;
  uns level;
  int long_continue;
  struct main_rec_io rio;
};

static void
close_fd(struct fds *fd)
{
  rec_io_del(&fd->rio);
  close(fd->fdnum);
  clist_remove(&fd->node);
  if (clist_empty(&filedescriptors))
    main_shut_down();
}


static void
do_msg (struct fds *fd, char *l_msg, int long_continue)
{
  msg(fd->level, "%s%s", (fd->long_continue ? "... " : ""), l_msg);
  fd->long_continue = long_continue;
}

static uns
handle_read(struct main_rec_io *r)
{
  char buf[max_line + 5];
  byte *eol = memchr((char *)r->read_rec_start + r->read_prev_avail, '\n', r->read_avail - r->read_prev_avail);
  if (eol == NULL) {
    if (r->read_avail >= max_line) {
      memcpy(buf, r->read_rec_start, max_line);
      memcpy(buf + max_line, " ...", 5);
      do_msg(r->data, buf, 1);
      return max_line;
    } else
      return 0;
  }
  *eol = 0;
  byte *b = r->read_rec_start;
  while ((uns)(eol - b) > max_line) {
    char cc = b[max_line];
    b[max_line]=0;
    do_msg(r->data, b, 1);
    b[max_line]=cc;
    b+=max_line;
  }
  do_msg(r->data, (char *)b, 0);
  return eol - r->read_rec_start + 1;
}

static int
handle_notify(struct main_rec_io *r, int status)
{
  struct fds *fd = r->data;
  switch (status) {
    case RIO_ERR_READ:
    case RIO_EVENT_EOF:
      if (r->read_avail) {
        char buf[max_line + 10];
        memcpy(buf, r->read_rec_start, r->read_avail);
        memcpy(buf + r->read_avail, " [no eol]", 10);
        do_msg(r->data, buf, 0);
      } else if (fd->long_continue) {
	do_msg(r->data, "[no eol]", 0);
      }
      close_fd(fd);
      return HOOK_DONE;
    default:
      ASSERT(0);
  }
  return HOOK_IDLE;
}


static void
add_level_fd(int fdnum, int level)
{
  struct fds *fd = xmalloc_zero(sizeof(*fd));
  fd->level = level;
  fd->pipe[0] = -1;
  fd->pipe[1] = -1;
  fd->fdnum = fdnum;
  fd->rio.read_handler = handle_read;
  fd->rio.data = fd;
  fd->rio.notify_handler = handle_notify;
  fd->long_continue = 0;
  clist_add_tail(&filedescriptors, &fd->node);
}

static int
xdup(int fd)
{
  int rfd = dup(fd);
  if (rfd == -1)
    die("Cannot dup(): %m");
  DBG("  Dup(%i) -> %i", fd, rfd);
  return rfd;
}

static int
xdup2(int fd1, int fd2)
{
  int rfd = dup2(fd1, fd2);
  if (rfd == -1)
    die("Cannot dup2(): %m");
  DBG("  Dup2(%i, %i) -> %i", fd1, fd2, rfd);
  return rfd;
}

static void
xdupavoid(int *fd1, int fd2)
{
  DBG("Dupavoid: !%i -> %i", fd2, *fd1);
  int ofd = *fd1;
  if (ofd == fd2) {
    *fd1 = xdup(ofd);
    DBG("  Close: %i", ofd);
    close(ofd);
  }
}

static void
xdupto(int *fd1, int fd2)
{
  DBG("Dupto: %i -> %i", *fd1, fd2);
  if (*fd1 == fd2)
    return;
  DBG("  Close: %i", fd2);
  close(fd2);
  xdup2(*fd1, fd2);
  DBG("  Close: %i", *fd1);
  close(*fd1);
  *fd1 = fd2;
}

static void
set_cloexec_flag(int fd, int value)
{
  int flags = fcntl(fd, F_GETFD, 0);
  if (flags < 0)
    die("fcntl(..., F_GETFD, ...) : %m");
  flags = (value) ? flags | FD_CLOEXEC : flags & ~FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) < 0)
    die("fcntl(..., F_SETFD, ...) : %m");
}

/* The "+" stands for end at first argument (i.e. do not parse options after
   first argument.) */
#define MY_SHORT_OPTS "+" CF_SHORT_OPTS "f:n:l:ih"
const struct option my_long_opts[] = {
  CF_LONG_OPTS
  { "help", 0, 0, 'h'},
  { "input", 0, 0, 'i'},
  { "logfile", 1, 0, 'f'},
  { "logname", 1, 0, 'n'},
  { "descriptor", 1, 0, 'l'},
  { "nv", 0, 0, 'q'},
  { "nonverbose", 0, 0, 'q'},
  { "non-verbose", 0, 0, 'q'},
  { "non_verbose", 0, 0, 'q'},
  { "silent", 0, 0, 's'},
  { NULL, 0, 0, 0}
};

#undef CF_USAGE_TAB
#define CF_USAGE_TAB "\t   "
static char usage[] =
  "Usage:\n"
  "ucw-logoutput -h|--help\t\t   This help.\n"
  "ucw-logoutput <options> -i|--input   Read file descriptors and log them.\n"
  "\t\t\t\t   default: stdin at level I.\n"
  "ucw-logoutput <opts> [--] <cmd> [arguments for cmd ...]\n"
  "\t\t\t\t   Open file descriptors for writing for command <cmd> and log them.\n"
  "\t\t\t\t   default: stdout:I, stderr:W.\n\n"
  "Options:\n"
  CF_USAGE
  "-n, --logname <name>\t\t   Use <name> as program name in logs.\n"
  "-l, --descriptor <fdnum>:<level>   Open file descriptor <fdnum> and log it at level <level> (replaces defaults).\n"
  "-f, --logfile <logfile>\t\t   Log to file <logfile>.\n"
  "-q, --nv, --nonverbose\t\t   Suppress launching and successful finish messages.\n"
  "-s, --silent\t\t\t   Suppress launching message and all finish messages.\n"
  "\t\t\t\t   (i.e., no warning if it terminates with a nonzero exit code or by a signal)\n";

int
main(int argc, char **argv)
{
  int register_default = 1;
  int loginput = 0;
  char *logfile = NULL;
  char *logname = NULL;
  struct fds *stderrfd = NULL;
  int help = 0;

  log_init("ucw-logoutput");
  clist_init(&filedescriptors);
  cf_declare_section("LogOutput", &cfsec_logoutput, 0);

  while (1) {
    int opt = cf_getopt(argc, argv, MY_SHORT_OPTS, my_long_opts, NULL);
    switch (opt) {
      case -1:
	goto opt_done;

      case 'h':
	help = 1;
	break;

      case 'i':
	loginput = 1;
	break;

      case 'f':
	logfile = optarg;
	break;

      case 'n':
	logname = optarg;
	break;

      case 'l':
	{
	  char *c = optarg;

	  register_default = 0;
	  int fdnum = 0;
	  int parseerror = 0;
	  if ( (c[0]<'0') || (c[0] > '9') )
	    parseerror = 1;
	  while ( (!parseerror) && (c[0] >= '0') && (c[0] <= '9') )
	    { fdnum = fdnum*10 + c[0] - '0'; c++; }
	  if ( (!parseerror) && (c[0] != ':') )
	    parseerror = 1;
	  c++;
	  if ( (!parseerror) && (c[0] == 0) )
	    parseerror = 1;
	  if ( (!parseerror) && (c[1] != 0) )
	    parseerror = 1;
	  if (parseerror) die("Bad argument `%s' to -l, expects number:letter.", optarg);

	  uns level = 0;
	  while (level < L_MAX && LS_LEVEL_LETTER(level) != c[0])
	    level++;
	  if (level >= L_MAX)
	    die("Unknown logging level `%s'", c);

	  add_level_fd(fdnum, level);
	}
	break;

      case 's':
	nonzero_status_message = 0;
	/* fallthrough */
      case 'q':
	launch_finish_messages = 0;
	break;

      default:
	optind--;
	goto opt_done;
    }
  }
opt_done:

  if (!help) {
    if (loginput && (optind < argc))
      die("No cmd is allowed for -i. Use -h for help.");

    if ((!loginput) && (optind >= argc)) {
      msg(L_FATAL, "Either command or --input expected.");
      help = 2;
    }
  }
  if (help) {
    write(2, usage, sizeof(usage));
    return (help == 1) ? 0 : 1;
  }

  if (register_default) {
    if (loginput) {
      add_level_fd(0, L_INFO);
    } else {
      add_level_fd(1, L_INFO);
      add_level_fd(2, L_WARN);
    }
  }

  if (loginput) {
    /* Just check, that we don't want open stderr for reading. */
    CLIST_FOR_EACH(struct fds *, fd, filedescriptors) {
      if (fd->fdnum == 2)
        die("Stderr is reserved for output");
    }
  } else {
    /* Open all filedescriptors and their duplicates. */
    CLIST_FOR_EACH(struct fds *, fd, filedescriptors) {
      CLIST_FOR_EACH(struct fds *, fdcheck, filedescriptors) {
        /* We do a dummy check for collisions of filedescriptors. */
        if (fdcheck == fd)
	  break;
        if (fdcheck->fdnum == fd->fdnum) {
          die("Duplicate filedescriptor %i", fd->fdnum);
        }
        xdupavoid(fdcheck->pipe + 0, fd->fdnum);
        xdupavoid(fdcheck->pipe + 1, fd->fdnum);
      }
      if (pipe(fd->pipe) == -1)
        die("Cannot create pipe: %m");
      DBG("Pipe [%i, %i] for %i", fd->pipe[0], fd->pipe[1], fd->fdnum);
      xdupavoid(fd->pipe + 0, fd->fdnum);

      if (fd->fdnum == 2) {
        stderrfd = fd; //We need to redirect stderr later.
      } else {
        xdupto(fd->pipe + 1, fd->fdnum);
      }
      DBG("---> [%i, %i] for %i", fd->pipe[0], fd->pipe[1], fd->fdnum);
      set_cloexec_flag(fd->pipe[0], 1);
      set_cloexec_flag(fd->pipe[1], 0);
    }
  }

  /* Initialize main loop. */
  main_init();

  CLIST_FOR_EACH(struct fds *, fd, filedescriptors) {
    /* Our pipe is created, let fd->fdnum be the reading end. */
    if (!loginput)
      fd->fdnum = fd->pipe[0];
    fd->rio.read_rec_max = max_line + 1;
    rec_io_add(&fd->rio, fd->fdnum);
  }

  int pid = -1;
  if (!loginput) {
    /* Launch the child and close filedescriptors. */
    pid = fork();
    if (pid == -1)
      die("Cannot fork: %m");
    if (pid == 0 ) {
      /* Child */

      /* Move stderr where it should be. */
      if (stderrfd)
        xdupto(stderrfd->pipe + 1, 2);

      execvp(argv[optind], argv + optind);
      if (stderrfd) {
        /* We translate stderr, just print. */
        perror("Cannot exec child");
        return 127;
      }
      /* No stderr translation: use logging function. */
      die("Cannot exec child: %m");
    }

    /* Close writing filedescriptors. */
    CLIST_FOR_EACH(struct fds *, fd, filedescriptors) {
      close(fd->pipe[1]);
    }
  }

  /* Open logfile or stderr. */
  if (logfile) {
    log_file(logfile);
    close(2);
  }

  if (!loginput) {
    /* Inform about launching of the command. */
    int buflen = 0;
    for (int i = optind; i < argc; i++) buflen += strlen(argv[i]) + 1;
    char *buf = xmalloc(buflen);
    char *buf2 = buf;
    for (int i = optind; i < argc; i++) {
      strcpy(buf2, argv[i]);
      buf2 += strlen(argv[i]);
      buf2[0] = ' ';
      buf2++;
    }
    buf2[-1] = 0;

    if (launch_finish_messages)
      msg(L_INFO, "Launching command: %s", buf);
  }

  /* Set logname. */
  if (logname)
    log_init(logname);
  else if (!loginput)
    log_init(argv[optind]);

  /* Start reading from pipes. */
  CLIST_FOR_EACH(struct fds *, fd, filedescriptors)
    rec_io_start_read(&fd->rio);
  main_loop();

  if (!loginput) {
    /* Unset logname. */
    log_init("ucw-logoutput");

    /* Wait for status of the child and inform about finish. */
    int status;
    char buf[256];

    while (waitpid(pid, &status, 0) == -1) {
      if (errno != EINTR)
        die("Cannot wait for child: %m");
    }

    if (format_exit_status(buf, status)) {
      if (nonzero_status_message)
	msg(L_WARN, "Child %s", buf);
      return WIFEXITED(status) ? WEXITSTATUS(status) : 127;
    } else {
      if (launch_finish_messages)
	msg(L_INFO, "Child terminated successfully.");
      return 0;
    }
  }

  return 0;
}
