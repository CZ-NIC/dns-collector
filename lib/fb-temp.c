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

#include <unistd.h>
#include <sys/fcntl.h>

static byte *temp_prefix = "/tmp/temp";

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

#ifdef CONFIG_UCW_THREADS
#include <pthread.h>

static pthread_key_t temp_counter_key;

static void CONSTRUCTOR
temp_key_init(void)
{
  if (pthread_key_create(&temp_counter_key, NULL) < 0)
    die("Cannot create fbdir_queue_key: %m");
}

void
temp_file_name(byte *buf)
{
  int cnt = (int) pthread_getspecific(temp_counter_key);
  cnt++;
  pthread_setspecific(temp_counter_key, (void *) cnt);

  int pid = getpid();
#if 0
  /* FIXME: This is Linux-specific and not declared anywhere :( */
  int tid = gettid();
#else
  int tid = pid;
#endif
  if (pid == tid)
    sprintf(buf, "%s%d-%d", temp_prefix, pid, cnt);
  else
    sprintf(buf, "%s%d-%d-%d", temp_prefix, pid, tid, cnt);
}

#else

void
temp_file_name(byte *buf)
{
  static int cnt;
  sprintf(buf, "%s%d-%d", temp_prefix, (int)getpid(), cnt++);
}

#endif

struct fastbuf *
bopen_tmp(uns buflen)
{
  byte buf[TEMP_FILE_NAME_LEN];
  struct fastbuf *f;

  temp_file_name(buf);
  f = bopen(buf, O_RDWR | O_CREAT | O_TRUNC, buflen);
  bconfig(f, BCONFIG_IS_TEMP_FILE, 1);
  return f;
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
