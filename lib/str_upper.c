/*
 *	Sherlock Library -- Uppercase Map
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/chartype.h"

unsigned char _c_upper[256] = {
#define CHAR(code,upper,unacc,acc,cat) upper,
#include "lib/charmap.h"
#undef CHAR
};
