/*
 *	Sherlock Library -- Universal Sorter
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
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

static struct cfitem sorter_config[] = {
  { "Sorter",		CT_SECTION,	NULL },
  { "Trace",		CT_INT,		&sorter_trace },
  { "PresortBuffer",	CT_INT,		&sorter_presort_bufsize },
  { "StreamBuffer",	CT_INT,		&sorter_stream_bufsize },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR sorter_init_config(void)
{
  cf_register(sorter_config);
}

uns sorter_pass_counter;
