/*
 *  An experiment with parallel reading and writing of files.
 */

#include "lib/lib.h"
#include "lib/lfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  ASSERT(argc == 4);
  uns files = atol(argv[1]);
  uns bufsize = atol(argv[2]) * 1024;				// Kbytes
  uns rounds = (u64)atol(argv[3]) * 1024*1024 / bufsize;	// Mbytes
  int fd[files];
  byte *buf[files];

  log(L_INFO, "Initializing");
  for (uns i=0; i<files; i++)
    {
      byte name[16];
      sprintf(name, "tmp/ft-%d", i);
      fd[i] = sh_open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
      if (fd[i] < 0)
	die("Cannot create %s: %m", name);
      buf[i] = big_alloc(bufsize);
    }
  sync();

  log(L_INFO, "Writing %d files in parallel with %d byte buffers", files, bufsize);
  init_timer();
  u64 total = 0, total_rep = 0;
  for (uns r=0; r<rounds; r++)
    {
      for (uns i=0; i<files; i++)
	{
	  for (uns j=0; j<bufsize; j++)
	    buf[i][j] = r+i+j;
	  uns c = write(fd[i], buf[i], bufsize);
	  ASSERT(c == bufsize);
	  total += c;
          if (total >= total_rep + 1024*1024*1024)
	    {
	      printf("Wrote %d MB (round %d of %d)\r", (int)(total >> 20), r, rounds);
	      fflush(stdout);
	      total_rep = total;
	    }
	}
    }
  log(L_INFO, "Syncing");
  sync();
  uns ms = get_timer();
  log(L_INFO, "Spent %dms (%d MB/sec)", ms, (uns)(total/ms*1000/1048576));

  log(L_INFO, "Reading the files sequentially");
  total = total_rep = 0;
  for (uns i=0; i<files; i++)
    {
      lseek(fd[i], 0, SEEK_SET);
      for (uns r=0; r<rounds; r++)
	{
	  uns c = read(fd[i], buf[i], bufsize);
	  ASSERT(c == bufsize);
	  total += c;
          if (total >= total_rep + 1024*1024*1024)
	    {
	      printf("Read %d MB (file %d)\r", (int)(total >> 20), i);
	      fflush(stdout);
	      total_rep = total;
	    }
	}
      close(fd[i]);
    }
  ms = get_timer();
  log(L_INFO, "Spent %dms (%d MB/sec)", ms, (uns)(total/ms*1000/1048576));

  log(L_INFO, "Done");
  return 0;
}
