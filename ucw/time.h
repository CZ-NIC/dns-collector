/*
 *	UCW Library -- A Simple Millisecond Timer
 *
 *	(c) 2007--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_TIMER_H
#define _UCW_TIMER_H

/***
 * === Timestamps
 *
 * All timing functions in LibUCW are based on signed 64-bit timestamps
 * with millisecond precision (the <<basics:type_timestamp_t,`timestamp_t`>> type), which measure
 * time from an unspecified moment in the past. Depending on the compile-time
 * settings, that moment can be the traditional UNIX epoch, or for example
 * system boot if POSIX monotonic clock is used.
 ***/

timestamp_t get_timestamp(void);		/** Get current time as a millisecond timestamp. **/

/***
 * === Timers
 *
 * A timer is a very simple construct for measuring execution time.
 * It can be initialized and then read multiple times. Each read returns
 * the number of milliseconds elapsed since the previous read or initialization.
 ***/

void init_timer(timestamp_t *timer);		/** Initialize a timer. **/
uns get_timer(timestamp_t *timer);		/** Get the number of milliseconds since last init/get of a timer. **/
uns switch_timer(timestamp_t *oldt, timestamp_t *newt);	/** Stop ticking of one timer and resume another. **/

#endif
