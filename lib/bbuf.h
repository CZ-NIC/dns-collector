/*
 *	A simple growing buffer for byte-sized items.
 *
 *	(c) 2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_BBUF_H
#define _SHERLOCK_BBUF_H

#define	GBUF_TYPE	byte
#define	GBUF_PREFIX(x)	bb_##x
#include "lib/gbuf.h"

#endif
