/*
 *	Sherlock Library -- Cryptographically Safe Random Key Generator
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#include <fcntl.h>
#include <unistd.h>

void
randomkey(byte *buf, uns size)
{
  int fd;

  if ((fd = open("/dev/urandom", O_RDONLY, 0)) < 0)
    die("Unable to open /dev/urandom: %m");
  if (read(fd, buf, size) != (int) size)
    die("Error reading /dev/urandom: %m");
  close(fd);
}
