/*
 *	Sherlock Library -- Character Classes
 *
 *	(c) 1998 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/chartype.h"

unsigned char _c_cat[256] = {
#define CHAR(code,upper,unacc,acc,cat) cat,
#include "lib/charmap.h"
#undef CHAR
};
