/*
 *	Sherlock Library -- auto-resizable printf() functions
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _LIB_PRINTF_H
#define _LIB_PRINTF_H

#include <stdarg.h>

/* The following functions are NOT reentrable.  */

byte *vxprintf(char *msg, va_list v);
byte *xprintf(char *msg, ...) __attribute__((format(printf,1,2)));

#endif
