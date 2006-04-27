/*
 *	UCW Library -- Temporary Fastbufs
 *
 *	(c) 2002--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"

#include <unistd.h>
#include <sys/fcntl.h>

static byte *temp_template = "/tmp/temp%d.%d";

static struct cf_section temp_config = {
  CF_ITEMS {
    CF_STRING("Template", &temp_template),
    CF_END
  }
};

static void CONSTRUCTOR temp_init_config(void)
{
  cf_declare_section("Tempfiles", &temp_config, 0);
}

struct fastbuf *
bopen_tmp(uns buflen)
{
  byte buf[256];
  struct fastbuf *f;
  static uns temp_counter;

  sprintf(buf, temp_template, (int) getpid(), temp_counter++);
  f = bopen(buf, O_RDWR | O_CREAT | O_TRUNC, buflen);
  bconfig(f, BCONFIG_IS_TEMP_FILE, 1);
  return f;
}
