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

void daemon_init(struct daemon_params *dp);

void daemon_run(struct daemon_params *dp, void (*body)(struct daemon_params *dp));

void daemon_exit(struct daemon_params *dp);

#endif
