/*
 *	UCW Library -- Lowercase Map
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/chartype.h"

const unsigned char _c_lower[256] = {
#define CHAR(code,upper,lower,cat) lower,
#include "lib/char-map.h"
#undef CHAR
};