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

/* proctitle.c */

void setproctitle_init(int argc, char **argv);
void setproctitle(const char *msg, ...) FORMAT_CHECK(printf,1,2);
char *getproctitle(void);

/* exitstatus.c */

#define EXIT_STATUS_MSG_SIZE 32
int format_exit_status(char *msg, int stat);

/* runcmd.c */

int run_command(const char *cmd, ...);
void NONRET exec_command(const char *cmd, ...);
void echo_command(char *buf, int size, const char *cmd, ...);
int run_command_v(const char *cmd, va_list args);
void NONRET exec_command_v(const char *cmd, va_list args);
void echo_command_v(char *buf, int size, const char *cmd, va_list args);

#endif	/* !_UCW_PROCESS_H */
