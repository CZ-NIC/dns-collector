/*
 *	Sherlock Library -- Temporary Files
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib.h"

ulg
temprand(uns key)
{
  static int seeded = 0;
  ulg rand;

  if (!seeded)
    {
      seeded = 1;
      srand(getpid());
    }
  rand = random() << 1;
  rand += key * 0xdeadbeef;
  return rand;
}

void
open_temp(struct tempfile *tf, byte *tftype)
{
  int retry = 50;
  while (retry--)
    {
      sprintf(tf->name, TMP_DIR "/%s%08x", tftype, temprand(retry));
      tf->fh = open(tf->name, O_RDWR | O_CREAT | O_EXCL, 0666);
      if (tf->fh >= 0)
	return;
    }
  die("Unable to create temporary file");
}

void
delete_temp(struct tempfile *tf)
{
  close(tf->fh);
  unlink(tf->name);
}
