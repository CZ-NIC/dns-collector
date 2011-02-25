/*
 *	UCW Library -- Main Loop: Testing
 *
 *	(c) 2004--2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/mainloop.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef TEST

static struct main_process mp;
static struct main_block_io fin, fout;
static struct main_hook hook;
static struct main_timer tm;
static struct main_signal sg;
static int sig_counter;

static byte rb[16];

static void dread(struct main_block_io *bio)
{
  if (bio->rpos < bio->rlen)
    {
      msg(L_INFO, "Read EOF");
      block_io_del(bio);
    }
  else
    {
      msg(L_INFO, "Read done");
      block_io_read(bio, rb, sizeof(rb));
    }
}

static void derror(struct main_block_io *bio, int cause)
{
  msg(L_INFO, "Error: %m !!! (cause %d)", cause);
  block_io_del(bio);
}

static void dwrite(struct main_block_io *bio UNUSED)
{
  msg(L_INFO, "Write done");
}

static int dhook(struct main_hook *ho UNUSED)
{
  msg(L_INFO, "Hook called");
  if (sig_counter >= 3)
    return HOOK_SHUTDOWN;
  return 0;
}

static void dtimer(struct main_timer *tm)
{
  msg(L_INFO, "Timer tick");
  timer_add_rel(tm, 11000);
  timer_add_rel(tm, 10000);
}

static void dentry(void)
{
  log_fork();
  msg(L_INFO, "*** SUBPROCESS START ***");
  sleep(2);
  msg(L_INFO, "*** SUBPROCESS FINISH ***");
  exit(0);
}

static void dexit(struct main_process *pr)
{
  msg(L_INFO, "Subprocess %d exited with status %x", pr->pid, pr->status);
}

static void dsignal(struct main_signal *sg UNUSED)
{
  msg(L_INFO, "SIGINT received (send 3 times to really quit, or use Ctrl-\\)");
  sig_counter++;
}

int
main(void)
{
  log_init(NULL);
  main_init();

  fin.read_done = dread;
  fin.error_handler = derror;
  block_io_add(&fin, 0);
  block_io_read(&fin, rb, sizeof(rb));

  fout.write_done = dwrite;
  fout.error_handler = derror;
  block_io_add(&fout, 1);
  block_io_write(&fout, "Hello, world!\n", 14);

  hook.handler = dhook;
  hook_add(&hook);

  tm.handler = dtimer;
  timer_add_rel(&tm,  1000);

  sg.signum = SIGINT;
  sg.handler = dsignal;
  signal_add(&sg);

  mp.handler = dexit;
  if (!process_fork(&mp))
    dentry();

  main_debug();

  main_loop();
  msg(L_INFO, "Finished.");

  block_io_del(&fin);
  block_io_del(&fout);
  hook_del(&hook);
  signal_del(&sg);
  main_cleanup();
  return 0;
}

#endif
