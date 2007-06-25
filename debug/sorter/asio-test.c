/*
 *  An experiment with parallel reading and writing of files using ASIO.
 */

#include "lib/lib.h"
#include "lib/lfs.h"
#include "lib/asio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define COPY
#define DIRECT O_DIRECT

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

static struct asio_queue io_queue;

int main(int argc, char **argv)
{
  ASSERT(argc == 4);
  uns files = atol(argv[1]);
  uns bufsize = atol(argv[2]) * 1024;				// Kbytes
  u64 total_size = (u64)atol(argv[3]) * 1024*1024*1024;		// Gbytes
  u64 cnt, cnt_rep;
  uns cnt_ms;
  int fd[files];
  byte name[files][16];
  struct asio_request *req[files];

  init_timer(&timer);

  io_queue.buffer_size = bufsize;
  io_queue.max_writebacks = 2;
  asio_init_queue(&io_queue);

#ifdef COPY
  msg(L_INFO, "Creating input file");
  int in_fd = sh_open("tmp/ft-in", O_RDWR | O_CREAT | O_TRUNC | DIRECT, 0666);
  ASSERT(in_fd >= 0);
  ASSERT(!(total_size % bufsize));
  P_INIT;
  for (uns i=0; i<total_size/bufsize; i++)
    {
      struct asio_request *r = asio_get(&io_queue);
      r->op = ASIO_WRITE_BACK;
      r->fd = in_fd;
      r->len = bufsize;
      byte *xbuf = r->buffer;
      for (uns j=0; j<bufsize; j++)
	xbuf[j] = i+j;
      asio_submit(r);
      P_UPDATE(bufsize);
    }
  asio_sync(&io_queue);
  lseek(in_fd, 0, SEEK_SET);
  sync();
  P_FINAL;
#endif

  msg(L_INFO, "Initializing output files");
  for (uns i=0; i<files; i++)
    {
      sprintf(name[i], "tmp/ft-%d", i);
      fd[i] = sh_open(name[i], O_RDWR | O_CREAT | O_TRUNC | DIRECT, 0666);
      if (fd[i] < 0)
	die("Cannot create %s: %m", name[i]);
    }
  sync();
  get_timer(&timer);

  msg(L_INFO, "Writing %d MB to %d files in parallel with %d byte buffers", (int)(total_size >> 20), files, bufsize);
  P_INIT;
  for (uns i=0; i<files; i++)
    req[i] = asio_get(&io_queue);
  for (uns round=0; round<total_size/bufsize/files; round++)
    {
      for (uns i=0; i<files; i++)
	{
	  struct asio_request *r = req[i];
#ifdef COPY
	  struct asio_request *rr, *rd = asio_get(&io_queue);
	  rd->op = ASIO_READ;
	  rd->fd = in_fd;
	  rd->len = bufsize;
	  asio_submit(rd);
	  rr = asio_wait(&io_queue);
	  ASSERT(rr == rd && rd->status == (int)rd->len);
	  memcpy(r->buffer, rd->buffer, bufsize);
	  asio_put(rr);
#else
	  for (uns j=0; j<bufsize; j++)
	    r->buffer[j] = round+i+j;
#endif
	  r->op = ASIO_WRITE_BACK;
	  r->fd = fd[i];
	  r->len = bufsize;
	  asio_submit(r);
	  P_UPDATE(bufsize);
	  req[i] = asio_get(&io_queue);
	}
    }
  for (uns i=0; i<files; i++)
    asio_put(req[i]);
  asio_sync(&io_queue);
#ifdef COPY
  close(in_fd);
#endif
  msg(L_INFO, "Syncing");
  sync();
  P_FINAL;

  msg(L_INFO, "Reading the files sequentially");
  P_INIT;
  for (uns i=0; i<files; i++)
    {
      lseek(fd[i], 0, SEEK_SET);
      for (uns round=0; round<total_size/bufsize/files; round++)
	{
	  struct asio_request *rr, *r = asio_get(&io_queue);
	  r->op = ASIO_READ;
	  r->fd = fd[i];
	  r->len = bufsize;
	  asio_submit(r);
	  rr = asio_wait(&io_queue);
	  ASSERT(rr == r && r->status == (int)bufsize);
	  asio_put(r);
	  P_UPDATE(bufsize);
	}
      close(fd[i]);
    }
  P_FINAL;

  for (uns i=0; i<files; i++)
    unlink(name[i]);
#ifdef COPY
  unlink("tmp/ft-in");
#endif

  asio_cleanup_queue(&io_queue);
  msg(L_INFO, "Done");
  return 0;
}
