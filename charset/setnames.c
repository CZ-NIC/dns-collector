/*
 *	Character Set Conversion Library 1.0 -- Character Set Names
 *
 *	(c) 1998--2005 Martin Mares <mj@ucw.cz>
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License.
 */

#include "ucw/lib.h"
#include "charset/charconv.h"

#include <string.h>

/* Names according to RFC 1345 (see http://www.iana.org/assignments/character-sets) */

static const char *cs_names[] = {
	"US-ASCII",
	"ISO-8859-1",
	"ISO-8859-2",
	"ISO-8859-3",
	"ISO-8859-4",
	"ISO-8859-5",
	"ISO-8859-6",
	"ISO-8859-7",
	"ISO-8859-8",
	"ISO-8859-9",
	"ISO-8859-10",
	"ISO-8859-11",
	"ISO-8859-13",
	"ISO-8859-14",
	"ISO-8859-15",
	"ISO-8859-16",
	"windows-1250",
	"windows-1251",
	"windows-1252",
	"x-kam-cs",
	"CSN_369103",
	"cp852",
	"x-mac-ce",
	"x-cork",
	"utf-8",
	"utf-16be",
	"utf-16le"
};

int
find_charset_by_name(const char *c)
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
    return (char *)cs_names[i];
}
