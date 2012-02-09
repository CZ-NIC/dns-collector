/*
 *	UCW Library -- Signal Handling
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_SIGHANDLER_H
#define _UCW_SIGHANDLER_H

typedef int (*ucw_sighandler_t)(int);	// gets signum, returns nonzero if abort() should be called

void handle_signal(int signum);
void unhandle_signal(int signum);
ucw_sighandler_t set_signal_handler(int signum, ucw_sighandler_t newh);

#endif	/* !_UCW_SIGHANDLER_H */
