/*
 *	Sherlock Library -- Character Classes
 *
 *	(c) 1998 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include "lib/chartype.h"

unsigned char _c_cat[256] = {
#define CHAR(code,upper,unacc,acc,cat) cat,
#include "lib/charmap.h"
#undef CHAR
};
