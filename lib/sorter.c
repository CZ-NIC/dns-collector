/*
 *	Sherlock Library -- Universal Sorter
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"

#include <unistd.h>
#include <sys/fcntl.h>

#define SORT_DECLARE_ONLY
#include "lib/sorter.h"

uns sorter_trace;
uns sorter_presort_bufsize = 65536;
uns sorter_stream_bufsize = 65536;
static byte *sorter_template = "/tmp/sort%d.%d";

static struct cfitem sorter_config[] = {
  { "Sorter",		CT_SECTION,	NULL },
  { "Trace",		CT_INT,		&sorter_trace },
  { "PresortBuffer",	CT_INT,		&sorter_presort_bufsize },
  { "StreamBuffer",	CT_INT,		&sorter_stream_bufsize },
  { "TempLate",		CT_STRING,	&sorter_template },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR sorter_init_config(void)
{
  cf_register(sorter_config);
}

uns sorter_pass_counter;
uns sorter_file_counter;

struct fastbuf *
sorter_open_tmp(void)
{
  byte buf[256];
  struct fastbuf *f;

  sprintf(buf, sorter_template, (int) getpid(), sorter_file_counter++);
  f = bopen(buf, O_RDWR | O_CREAT | O_EXCL, sorter_stream_bufsize);
  f->is_temp_file = 1;
  return f;
}
