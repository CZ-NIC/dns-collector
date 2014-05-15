/*
 *	UCW Library -- Daemonization
 *
 *	(c) 2012--2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_DAEMON_H
#define _UCW_DAEMON_H

#include <sys/types.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define daemon_control ucw_daemon_control
#define daemon_exit ucw_daemon_exit
#define daemon_init ucw_daemon_init
#define daemon_resolve_ugid ucw_daemon_resolve_ugid
#define daemon_run ucw_daemon_run
#define daemon_switch_ugid ucw_daemon_switch_ugid
#endif

/** Parameters passed to the daemon helper. **/
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

/** Flags passed to the daemon helper. **/
enum daemon_flags {
  DAEMON_FLAG_PRESERVE_CWD = 1,		// Skip chdir("/")
  DAEMON_FLAG_SIMULATE = 2,		// Simulate daemonization (avoid fork etc.)
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
 *
 * When `DAEMON_FLAG_SIMULATE` is set, it justs calls @body. This is useful
 * for running of daemons in a debugger.
 **/
void daemon_run(struct daemon_params *dp, void (*body)(struct daemon_params *dp));

/**
 * Clean up when the daemon is about to exit. It removes the PID file.
 **/
void daemon_exit(struct daemon_params *dp);

/**
 * Parse `run_as_user` and `run_as_group` and remember the results in internal fields.
 * This is called automatically by daemon_init(), but also provided as a separate
 * function in case you want to use daemon_switch_ugid(). Upon parse error, it calls die().
 **/
void daemon_resolve_ugid(struct daemon_params *dp);

/**
 * Switch user and group as specified by the `run_as_user` and `run_as_group`.
 * This is performed automatically by daemon_run(), but sometimes you might want to
 * switch the user and group separately. In this case, you have to call daemon_resolve_ugid()
 * beforehand.
 **/
void daemon_switch_ugid(struct daemon_params *dp);

#define DAEMON_ERR_LEN 256

/** Parameters passed to @daemon_control() **/
struct daemon_control_params {
  const char *pid_file;		// A path to PID file
  const char *guard_file;	// A path to guard file
  int action;			// Action to perform (DAEMON_CONTROL_xxx)
  char * const *argv;		// Daemon's arguments, NULL-terminated (for DAEMON_CONTROL_START)
  int signal;			// Signal to send (for DAEMON_CONTROL_SIGNAL)
  char error_msg[DAEMON_ERR_LEN];	// A detailed error message returned (for DAEMON_STATUS_ERROR)
};

enum daemon_control_action {
  DAEMON_CONTROL_CHECK = 1,
  DAEMON_CONTROL_START,
  DAEMON_CONTROL_STOP,
  DAEMON_CONTROL_SIGNAL,
};

/**
 * Perform an action on a daemon:
 *
 * * `DAEMON_CONTROL_START` to start the daemon
 * * `DAEMON_CONTROL_STOP` to stop the daemon (send `SIGTERM` or `dc->signal` if non-zero)
 * * `DAEMON_CONTROL_CHECK` to check that the daemon is running
 * * `DAEMON_CONTROL_SIGNAL` to send a signal to the daemon (send `SIGHUP` or `dc->signal` if non-zero)
 *
 * The function returns a status code:
 *
 * * `DAEMON_STATUS_OK` if the action has been performed successfully
 * * `DAEMON_STATUS_ALREADY_DONE` if the daemon is already in the requested state
 * * `DAEMON_STATUS_NOT_RUNNING` if the action failed, because the daemon is not running
 * * `DAEMON_STATUS_ERROR` if the action failed for some other reason (in this case,
 *   `dc->error_msg` contains a full error message)
 * * `DAEMON_STATUS_STALE` if the daemon was in an undefined state (e.g., a stale PID file);
 *   for `DAEMON_CONTROL_START`, it means success
 **/
enum daemon_control_status daemon_control(struct daemon_control_params *dc);

// XXX: Also used as exit codes of the ucw-daemon-control utility.
enum daemon_control_status {
  DAEMON_STATUS_OK = 0,
  DAEMON_STATUS_ALREADY_DONE = 100,
  DAEMON_STATUS_NOT_RUNNING = 101,
  DAEMON_STATUS_ERROR = 102,
  DAEMON_STATUS_STALE = 103,
};

#endif
