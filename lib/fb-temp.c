/*
 *	UCW Library -- Temporary Fastbufs
 *
 *	(c) 2002--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"
#include "lib/threads.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>

static char *temp_prefix = "/tmp/temp";

static struct cf_section temp_config = {
  CF_ITEMS {
    CF_STRING("Prefix", &temp_prefix),
    CF_END
  }
};

static void CONSTRUCTOR temp_global_init(void)
{
  cf_declare_section("Tempfiles", &temp_config, 0);
}

void
temp_file_name(char *buf)
{
  struct ucwlib_context *ctx = ucwlib_thread_context();
  int cnt = ++ctx->temp_counter;
  int pid = getpid();
  if (ctx->thread_id == pid)
    sprintf(buf, "%s%d-%d", temp_prefix, pid, cnt);
  else
    sprintf(buf, "%s%d-%d-%d", temp_prefix, pid, ctx->thread_id, cnt);
}

struct fastbuf *
bopen_tmp(uns buflen)
{
  char buf[TEMP_FILE_NAME_LEN];
  struct fastbuf *f;

  // FIXME: This needs cleanup and merging with other bopen functions.
  temp_file_name(buf);
  f = bopen(buf, O_RDWR | O_CREAT | O_TRUNC, buflen);
  bconfig(f, BCONFIG_IS_TEMP_FILE, 1);
  return f;
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

#include "lib/getopt.h"

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
