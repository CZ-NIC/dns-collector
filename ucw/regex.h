/*
 *	UCW Library -- Interface to Regular Expression Libraries
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_REGEX_H
#define _UCW_REGEX_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define rx_compile ucw_rx_compile
#define rx_free ucw_rx_free
#define rx_match ucw_rx_match
#define rx_subst ucw_rx_subst
#endif

typedef struct regex regex;

regex *rx_compile(const char *r, int icase);
void rx_free(regex *r);
int rx_match(regex *r, const char *s);
int rx_subst(regex *r, const char *by, const char *src, char *dest, uns destlen);

#endif
