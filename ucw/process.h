/*
 *	UCW Library -- Processes
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_PROCESS_H
#define _UCW_PROCESS_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define echo_command ucw_echo_command
#define echo_command_v ucw_echo_command_v
#define exec_command ucw_exec_command
#define exec_command_v ucw_exec_command_v
#define format_exit_status ucw_format_exit_status
#define getproctitle ucw_getproctitle
#define run_command ucw_run_command
#define run_command_v ucw_run_command_v
#define setproctitle ucw_setproctitle
#define setproctitle_init ucw_setproctitle_init
#endif

/* proctitle.c */

// Must be called before parsing of arguments
void setproctitle_init(int argc, char **argv);
void setproctitle(const char *msg, ...) FORMAT_CHECK(printf,1,2);
char *getproctitle(void);

/* exitstatus.c */

#define EXIT_STATUS_MSG_SIZE 64
int format_exit_status(char *msg, int stat);

/* runcmd.c */

int run_command(const char *cmd, ...);
void NONRET exec_command(const char *cmd, ...);
void echo_command(char *buf, int size, const char *cmd, ...);
int run_command_v(const char *cmd, va_list args);
void NONRET exec_command_v(const char *cmd, va_list args);
void echo_command_v(char *buf, int size, const char *cmd, va_list args);

#endif	/* !_UCW_PROCESS_H */
