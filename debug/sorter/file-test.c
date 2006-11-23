/*
 *  An experiment with parallel reading and writing of files.
 */

#include "lib/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  ASSERT(argc == 4);
  uns files = atol(argv[1]);
  uns bufsize = atol(argv[2]);
  uns rounds = atol(argv[3]);
  int fd[files];
  byte *buf[files];

  log(L_INFO, "Initializing");
  for (uns i=0; i<files; i++)
    {
      byte name[16];
      sprintf(name, "tmp/ft-%d", i);
      fd[i] = open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
      if (fd[i] < 0)
	die("Cannot create %s: %m", name);
      buf[i] = big_alloc(bufsize);
    }
  sync();

  log(L_INFO, "Writing %d files in parallel with %d byte buffers", files, bufsize);
  for (uns r=0; r<rounds; r++)
    {
      log(L_INFO, "\tRound %d", r);
      for (uns i=0; i<files; i++)
	{
	  for (uns j=0; j<bufsize; j++)
	    buf[i][j] = r+i+j;
	  uns c = write(fd[i], buf[i], bufsize);
	  ASSERT(c == bufsize);
	}
    }
  log(L_INFO, "Syncing");
  sync();

  log(L_INFO, "Reading the files sequentially");
  for (uns i=0; i<files; i++)
    {
      lseek(fd[i], 0, SEEK_SET);
      for (uns r=0; r<rounds; r++)
	{
	  uns c = read(fd[i], buf[i], bufsize);
	  ASSERT(c == bufsize);
	}
      close(fd[i]);
    }

  log(L_INFO, "Done");
  return 0;
}
