/*
 *	Sherlock Utilities -- A Simple Logger for use in shell scripts
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdio.h>
#include <string.h>

int
main(int argc, char **argv)
{
  byte buf[1024], *c;

  log_init("logger");
  if (argc < 3 || argc > 4 || strlen(argv[2]) != 1)
    die("Usage: logger [<logname>:]<progname> <level> [<text>]");
  if (c = strchr(argv[1], ':'))
    {
      *c++ = 0;
      log_init(c);
      log_file(argv[1]);
    }
  else
    log_init(argv[1]);
  if (argc > 3)
    log(argv[2][0], argv[3]);
  else
    while (fgets(buf, sizeof(buf), stdin))
      {
	c = strchr(buf, '\n');
	if (c)
	  *c = 0;
	log(argv[2][0], buf);
      }
  return 0;
}
