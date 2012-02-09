/*
 *	UCW Library -- File Sizes
 *
 *	(c) 1999--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/io.h"

ucw_off_t ucw_file_size(const char *name)
{
  int fd = ucw_open(name, O_RDONLY);
  if (fd < 0)
    die("Cannot open %s: %m", name);
  ucw_off_t len = ucw_seek(fd, 0, SEEK_END);
  close(fd);
  return len;
}
