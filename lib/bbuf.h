/*
 *	UCW Library -- A simple growing buffer for byte-sized items.
 *
 *	(c) 2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_BBUF_H
#define _UCW_BBUF_H

#define	GBUF_TYPE	byte
#define	GBUF_PREFIX(x)	bb_##x
#include "lib/gbuf.h"

char *bb_ofs_vprintf(bb_t *bb, uns ofs, char *fmt, va_list args);
char *bb_ofs_printf(bb_t *bb, uns ofs, char *fmt, ...);

#endif
