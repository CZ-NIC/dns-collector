/*
 *	UCW Library -- Temporary Fastbufs
 *
 *	(c) 2002--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/conf.h"
#include "ucw/fastbuf.h"
#include "ucw/threads.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>

static char *temp_prefix;
static int public_dir = 1;

static struct cf_section temp_config = {
  CF_ITEMS {
    CF_STRING("Prefix", &temp_prefix),
    CF_INT("PublicDir", &public_dir),
    CF_END
  }
};

static void CONSTRUCTOR temp_global_init(void)
{
  cf_declare_section("Tempfiles", &temp_config, 0);
}

void
temp_file_name(char *name_buf, int *open_flags)
{
  char *prefix;
  int free_prefix = 0;
  if (temp_prefix)
    prefix = temp_prefix;
  else
    {
      char *env = getenv("TMPDIR");
      if (env)
        {
          prefix = xmalloc(strlen(env) + 6);
          sprintf(prefix, "%s/temp", env);
          free_prefix = 1;
        }
      else
        prefix = "/tmp/temp";
    }
  if (public_dir)
    {
      struct timeval tv;
      if (gettimeofday(&tv, NULL))
        die("Could not generate temp file name: %m");
      sprintf(name_buf, "%s-%u", prefix, (uns) tv.tv_usec);
      if (open_flags)
        *open_flags = O_EXCL;
    }
  else
    {
      struct ucwlib_context *ctx = ucwlib_thread_context();
      int cnt = ++ctx->temp_counter;
      int pid = getpid();
      if (ctx->thread_id == pid)
        sprintf(name_buf, "%s%d-%d", temp_prefix, pid, cnt);
      else
        sprintf(name_buf, "%s%d-%d-%d", temp_prefix, pid, ctx->thread_id, cnt);
      if (open_flags)
        *open_flags = 0;
    }
  if (free_prefix)
    xfree(prefix);
}

struct fastbuf *
bopen_tmp_file(struct fb_params *params)
{
  char name[TEMP_FILE_NAME_LEN];
  int fd = open_tmp(name, O_RDWR | O_CREAT | O_TRUNC, 0600);
  struct fastbuf *fb = bopen_fd_name(fd, params, name);
  bconfig(fb, BCONFIG_IS_TEMP_FILE, 1);
  return fb;
}

int
open_tmp(char *name_buf, int open_flags, int mode)
{
  int create_flags, fd, retry = 10;
  do
    {
      temp_file_name(name_buf, &create_flags);
      fd = open(name_buf, open_flags | create_flags, mode);
    }
  while (fd < 0 && errno == EEXIST && retry --);
  if (fd < 0)
    die("Unable to create temp file %s: %m", name_buf);
  return fd;
}

struct fastbuf *
bopen_tmp(uns buflen)
{
  return bopen_tmp_file(&(struct fb_params){ .type = FB_STD, .buffer_size = buflen });
}

void bfix_tmp_file(struct fastbuf *fb, const char *name)
{
  int was_temp = bconfig(fb, BCONFIG_IS_TEMP_FILE, 0);
  ASSERT(was_temp == 1);
  if (rename(fb->name, name))
    die("Cannot rename %s to %s: %m", fb->name, name);
  bclose(fb);
}

#ifdef TEST

#include "ucw/getopt.h"

int main(int argc, char **argv)
{
  log_init(NULL);
  if (cf_getopt(argc, argv, CF_SHORT_OPTS, CF_NO_LONG_OPTS, NULL) >= 0)
    die("Hey, whaddya want?");

  struct fastbuf *f = bopen_tmp(65536);
  bputsn(f, "Hello, world!");
  bclose(f);
  return 0;
}

#endif
