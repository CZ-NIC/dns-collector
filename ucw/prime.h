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

#include <ucw/lib.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define isprime ucw_isprime
#define next_table_prime ucw_next_table_prime
#define nextprime ucw_nextprime
#define prev_table_prime ucw_prev_table_prime
#endif

/* prime.c */

/**
 * Return a non-zero value iff @x is a prime number.
 * The time complexity is `O(sqrt(x))`.
 **/
int isprime(uint x);

/**
 * Return some prime greater than @x. The function does not checks overflows, but it should
 * be safe at least for @x lower than `1U << 31`.
 * If the Cramer's conjecture is true, it should have complexity `O(sqrt(x) * log(x)^2)`.
 **/
uint nextprime(uint x);

/* primetable.c */

/**
 * Quickly lookup a precomputed table to return a prime number greater than @x.
 * Returns zero if there is no such prime (we guarantee the existance of at
 * least one prime greater than `1U << 31` in the table).
 **/
uint next_table_prime(uint x);

/**
 * Quickly lookup a precomputed table to return a prime number smaller than @x.
 * Returns zero if @x is smaller than `7`.
 **/
uint prev_table_prime(uint x);

#endif // _UCW_PRIME_H
