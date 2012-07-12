/*
 *	UCW Library -- Daemonization
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_DAEMON_H
#define _UCW_DAEMON_H

#include <sys/types.h>

struct daemon_params {
  uns flags;				// DAEMON_FLAG_xxx
  const char *pid_file;			// A path to PID file (optional)
  const char *run_as_user;		// User name or "#uid" (optional)
  const char *run_as_group;		// Group name or "#gid" (optional)

  // Internal
  uid_t run_as_uid;
  uid_t run_as_gid;
  int want_setuid;
  int want_setgid;
  int pid_fd;
};

enum daemon_flags {
  DAEMON_FLAG_PRESERVE_CWD = 1,		// Skip chdir("/")
};

/**
 * Daemon initialization. Should be run after parsing of options.
 * It resolves the UID and GID to run with and locks the PID file.
 * Upon error, it calls @die().
 **/
void daemon_init(struct daemon_params *dp);

/**
 * Run the daemon. Should be run when everything is initialized. It forks off
 * a new process and does all necessary setup. Inside the new process, it calls
 * @body (and when it returns, it exits the process). In the original process, it writes
 * the PID file and returns.
 **/
void daemon_run(struct daemon_params *dp, void (*body)(struct daemon_params *dp));

/**
 * Clean up when the daemon is about to exit. It removes the PID file.
 **/
void daemon_exit(struct daemon_params *dp);

#endif
