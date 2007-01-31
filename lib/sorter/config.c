/*
 *	UCW Library -- Universal Sorter: Configuration
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/fastbuf.h"
#include "lib/sorter/common.h"

uns sorter_trace;
uns sorter_presort_bufsize = 65536;
uns sorter_stream_bufsize = 65536;

static struct cf_section sorter_config = {
  CF_ITEMS {
    CF_UNS("Trace", &sorter_trace),
    CF_UNS("PresortBuffer", &sorter_presort_bufsize),
    CF_UNS("StreamBuffer", &sorter_stream_bufsize),
    CF_END
  }
};

static void CONSTRUCTOR sorter_init_config(void)
{
  cf_declare_section("Sorter", &sorter_config, 0);
}
