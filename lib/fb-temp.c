/*
 *	Sherlock Library -- Temporary Fastbufs
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
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

static struct cfitem temp_config[] = {
  { "Tempfiles",	CT_SECTION,	NULL },
  { "Template",		CT_STRING,	&temp_template },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR temp_init_config(void)
{
  cf_register(temp_config);
}

struct fastbuf *
bopen_tmp(uns bufsize)
{
  byte buf[256];
  struct fastbuf *f;
  static uns temp_counter;

  sprintf(buf, temp_template, (int) getpid(), temp_counter++);
  f = bopen(buf, O_RDWR | O_CREAT | O_EXCL, bufsize);
  f->is_temp_file = 1;
  return f;
}
