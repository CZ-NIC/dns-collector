/*
 *	Character Set Conversion Library 1.0 -- Character Set Names
 *
 *	(c) 1998--2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License.
 */

#include "lib/lib.h"
#include "charset/charconv.h"

#include <string.h>

static char *cs_names[] = {
	"US-ASCII",
	"ISO-8859-1",
	"ISO-8859-2",
	"windows-1250",
	"x-kam-cs",
	"CSN_369103",
	"cp852",
	"x-mac-ce",
	"utf-8"
};

int
find_charset_by_name(char *c)
{
	unsigned int i;

	for(i=0; i<CONV_NUM_CHARSETS; i++)
		if (!strcasecmp(cs_names[i], c))
			return i;
	return -1;
}

char *
charset_name(int i)
{
  if (i < 0 || i > CONV_NUM_CHARSETS)
    return "x-unknown";
  else
    return cs_names[i];
}
