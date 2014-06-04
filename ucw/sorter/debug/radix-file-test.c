/*
 *  An experiment with parallel reading and writing of files.
 *
 *  (c) 2007 Martin Mares <mj@ucw.cz>
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/io.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define COPY
#define DIRECT 0		// or O_DIRECT

static timestamp_t timer;

#define P_INIT do { cnt = 0; cnt_rep = 0; cnt_ms = 1; } while(0)
#define P_UPDATE(cc) do { \
  cnt += cc; \
  if (cnt >= cnt_rep) { cnt_ms += get_timer(&timer); \
    printf("%d of %d MB (%.2f MB/sec)\r", (int)(cnt >> 20), (int)(total_size >> 20), (double)cnt / 1048576 * 1000 / cnt_ms); \
    fflush(stdout); cnt_rep += 1<<26; } } while(0)
#define P_FINAL do { \
  cnt_ms += get_timer(&timer); \
  msg(L_INFO, "Spent %.3f sec (%.2f MB/sec)", (double)cnt_ms/1000, (double)cnt / 1048576 * 1000 / cnt_ms); \
} while(0)

int main(int argc, char **argv)
{
  uint files, bufsize;
  u64 total_size;
  if (argc != 4 ||
      cf_parse_int(argv[1], (int*) &files) ||
      cf_parse_int(argv[2], (int*) &bufsize) ||
      cf_parse_u64(argv[3], &total_size))
    {
      fprintf(stderr, "Usage: file-test <nr-files> <bufsize> <totalsize>\n");
      return 1;
    }
  u64 cnt, cnt_rep;
  uint cnt_ms;
  int fd[files];
  byte *buf[files], name[files][16];
  uint xbufsize = bufsize;					// Used for single-file I/O
  byte *xbuf = big_alloc(xbufsize);

  init_timer(&timer);

#ifdef COPY
  msg(L_INFO, "Creating input file");
  int in_fd = ucw_open("tmp/ft-in", O_RDWR | O_CREAT | O_TRUNC | DIRECT, 0666);
  ASSERT(in_fd >= 0);
  ASSERT(!(total_size % xbufsize));
  P_INIT;
  for (uint i=0; i<total_size/xbufsize; i++)
    {
      for (uint j=0; j<xbufsize; j++)
	xbuf[j] = i+j;
      uint c = write(in_fd, xbuf, xbufsize);
      ASSERT(c == xbufsize);
      P_UPDATE(c);
    }
  lseek(in_fd, 0, SEEK_SET);
  sync();
  P_FINAL;
#endif

  msg(L_INFO, "Initializing output files");
  for (uint i=0; i<files; i++)
    {
      sprintf(name[i], "tmp/ft-%d", i);
      fd[i] = ucw_open(name[i], O_RDWR | O_CREAT | O_TRUNC | DIRECT, 0666);
      if (fd[i] < 0)
	die("Cannot create %s: %m", name[i]);
      buf[i] = big_alloc(bufsize);
    }
  sync();
  get_timer(&timer);

  msg(L_INFO, "Writing %d MB to %d files in parallel with %d byte buffers", (int)(total_size >> 20), files, bufsize);
  P_INIT;
  for (uint r=0; r<total_size/bufsize/files; r++)
    {
      for (uint i=0; i<files; i++)
	{
#ifdef COPY
	  uint ci = read(in_fd, buf[i], bufsize);
	  ASSERT(ci == bufsize);
#else
	  for (uint j=0; j<bufsize; j++)
	    buf[i][j] = r+i+j;
#endif
	  uint c = write(fd[i], buf[i], bufsize);
	  ASSERT(c == bufsize);
	  P_UPDATE(c);
	}
    }
#ifdef COPY
  close(in_fd);
#endif
  msg(L_INFO, "Syncing");
  sync();
  P_FINAL;

  msg(L_INFO, "Reading the files sequentially");
  P_INIT;
  for (uint i=0; i<files; i++)
    {
      lseek(fd[i], 0, SEEK_SET);
      for (uint r=0; r<total_size/xbufsize/files; r++)
	{
	  uint c = read(fd[i], xbuf, xbufsize);
	  ASSERT(c == xbufsize);
	  P_UPDATE(c);
	}
      close(fd[i]);
    }
  P_FINAL;

  for (uint i=0; i<files; i++)
    unlink(name[i]);
#ifdef COPY
  unlink("tmp/ft-in");
#endif
  msg(L_INFO, "Done");
  return 0;
}
