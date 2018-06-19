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

#ifdef CONFIG_UCW_CLEAN_ABI
#define get_timer ucw_get_timer
#define get_timestamp ucw_get_timestamp
#define init_timer ucw_init_timer
#define switch_timer ucw_switch_timer
#define timestamp_type ucw_timestamp_type
#endif

/***
 * === Timestamps
 *
 * All timing functions in LibUCW are based on signed 64-bit timestamps
 * with millisecond precision (the <<basics:type_timestamp_t,`timestamp_t`>> type), which measure
 * time from an unspecified moment in the past. Depending on the compile-time
 * settings, that moment can be the traditional UNIX epoch, or for example
 * system boot if POSIX monotonic clock is used.
 ***/

/* time-stamp.c */

timestamp_t get_timestamp(void);		/** Get current time as a millisecond timestamp. **/

/* time-conf.c */

/**
 * A user type for parsing of time intervals in configuration files.
 * It is specified as fractional seconds and internally converted to
 * a <<basics:type_timestamp_t,`timestamp_t`>>. When conversion of
 * a non-zero value yields zero, an error is raised.
 **/
extern struct cf_user_type timestamp_type;

/***
 * === Timers
 *
 * A timer is a very simple construct for measuring execution time.
 * It can be initialized and then read multiple times. Each read returns
 * the number of milliseconds elapsed since the previous read or initialization.
 ***/

/* time-timer.c */

void init_timer(timestamp_t *timer);		/** Initialize a timer. **/
uint get_timer(timestamp_t *timer);		/** Get the number of milliseconds since last init/get of a timer. **/
uint switch_timer(timestamp_t *oldt, timestamp_t *newt);	/** Stop ticking of one timer and resume another. **/

#endif
