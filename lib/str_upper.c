/*
 *	Sherlock Library -- Uppercase Map
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include "lib/chartype.h"

unsigned char _c_upper[256] = {
#define CHAR(code,upper,unacc,acc,cat) upper,
#include "lib/charmap.h"
#undef CHAR
};
