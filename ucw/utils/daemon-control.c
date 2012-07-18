/*
 *	A Simple Utility for Controlling Daemons
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *
 *	For more information, see ucw/doc/daemon.txt.
 *
 *	Return codes:
 *	100	already done
 *	101	not running
 *	102	error
 */

#include <ucw/lib.h>
#include <ucw/daemon.h>
#include <ucw/signames.h>
#include <ucw/string.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

static int action;

static struct option options[] = {
  { "pid-file",		required_argument,	NULL,		'p' },
  { "guard-file",	required_argument,	NULL,		'g' },
  { "signal",		required_argument,	NULL,		's' },
  { "start",		no_argument,		&action,	DAEMON_CONTROL_START },
  { "stop",		no_argument,		&action,	DAEMON_CONTROL_STOP },
  { "check",		no_argument,		&action,	DAEMON_CONTROL_CHECK },
  { "reload",		no_argument,		&action,	DAEMON_CONTROL_SIGNAL },
  { NULL,		no_argument,		NULL,		0 }
};

static void NONRET
usage(void)
{
  fputs("\n\
Usage: daemon-control --start <options> -- <daemon> <args>\n\
   or: daemon-control --stop <options>\n\
   or: daemon-control --reload <options>\n\
   or: daemon-control --check <options>\n\
\n\
Options:\n\
--pid-file <name>	Name of PID file for this daemon (mandatory)\n\
--guard-file <name>	Name of guard file (default: derived from --pid-file)\n\
--signal <sig>		Send a signal of a given name or number\n\
			Default: SIGTERM for --stop, SIGHUP for --reload\n\
\n\
Exit codes:\n\
0			Successfully completed\n\
1			Invalid arguments\n\
100			The action was null (e.g., --stop on a stopped daemon)\n\
101			The daemon was not running (on --reload or --check)\n\
102			The action has failed (error message was printed to stderr)\n\
103			The daemon was in an undefined state (e.g., stale PID file)\n\
", stderr);
  exit(1);
}

int
main(int argc, char **argv)
{
  int c, sig;
  struct daemon_control_params dc = { };

  while ((c = getopt_long(argc, argv, "", options, NULL)) >= 0)
    switch (c)
      {
      case 0:
	break;
      case 'p':
	dc.pid_file = optarg;
	break;
      case 'g':
	dc.guard_file = optarg;
	break;
      case 's':
	sig = sig_name_to_number(optarg);
	if (sig < 0)
	  {
	    fprintf(stderr, "%s: Unknown signal %s\n", argv[0], optarg);
	    return 1;
	  }
	dc.signal = sig;
	break;
      default:
	usage();
      }
  if (!dc.pid_file || !action)
    usage();
  dc.action = action;

  if (action == DAEMON_CONTROL_START)
    {
      if (optind >= argc)
	usage();
      dc.argv = argv + optind;
    }
  else if (optind < argc)
    usage();

  if (!dc.guard_file)
    {
      if (!str_has_suffix(dc.pid_file, ".pid"))
	{
	  fprintf(stderr, "%s: For automatic choice of --guard-file, the --pid-file must end with `.pid'\n", argv[0]);
	  return 1;
	}
      int len = strlen(dc.pid_file);
      char *buf = xmalloc(len + 2);
      sprintf(buf, "%.*s.lock", len-4, dc.pid_file);
      dc.guard_file = buf;
    }

  int err = daemon_control(&dc);
  if (err == DAEMON_STATUS_ERROR)
    fprintf(stderr, "%s: %s\n", argv[0], dc.error_msg);
  return err;
}
