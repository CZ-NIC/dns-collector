/*
 *	UCW Library -- Syncing Directories
 *
 *	(c) 2004 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#include <fcntl.h>
#include <unistd.h>

void
sync_dir(byte *name)
{
  int fd = open(name, O_RDONLY | O_DIRECTORY);
  if (fd < 0)
    goto err;
  int err = fsync(fd);
  close(fd);
  if (err >= 0)
    return;
 err:
  log(L_ERROR, "Unable to sync directory %s: %m", name);
}
