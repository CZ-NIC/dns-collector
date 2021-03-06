/*
 *	UCW Library -- Careful Read/Write
 *
 *	(c) 2004--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/io.h>

#include <unistd.h>

/*
 *  Reads and writes on sockets and pipes can return partial results,
 *  so we implement an iterated read/write call.
 */

int
careful_read(int fd, void *buf, size_t len)
{
  byte *pos = buf;
  while (len)
    {
      ssize_t l = read(fd, pos, len);
      if (l < 0)
	return -1;
      if (!l)
	return 0;
      pos += l;
      len -= l;
    }
  return 1;
}

int
careful_write(int fd, const void *buf, size_t len)
{
  const byte *pos = buf;
  while (len)
    {
      ssize_t l = write(fd, pos, len);
      if (l < 0)
	return -1;
      if (!l)
	return 0;
      pos += l;
      len -= l;
    }
  return 1;
}
