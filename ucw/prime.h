/*
 *	The UCW Library -- Prime numbers
 *
 *	(c) 2008 Michal Vaner <vorner@ucw.cz>
 *
 *	Code taken from ucw/lib.h by:
 *
 *	(c) 1997--2008 Martin Mares <mj@ucw.cz>
 *	(c) 2005 Tomas Valla <tom@ucw.cz>
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_PRIME_H
#define _UCW_PRIME_H

#include "ucw/lib.h"

/* prime.c */

int isprime(uns x);
uns nextprime(uns x);

/* primetable.c */

uns next_table_prime(uns x);
uns prev_table_prime(uns x);

#endif // _UCW_PRIME_H
