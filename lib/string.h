/*
 *	UCW Library -- String Routines
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *	(c) 2007--2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_STRING_H
#define _UCW_STRING_H

char *str_unesc(char *dest, const char *src);
char *str_format_flags(char *dest, const char *fmt, uns flags);

#endif
