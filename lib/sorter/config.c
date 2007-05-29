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
uns sorter_presort_bufsize = 65536;		/* FIXME: kill after removing the old sorter */
uns sorter_stream_bufsize = 65536;
u64 sorter_bufsize = 65536;
uns sorter_debug;
uns sorter_min_radix_bits;
uns sorter_max_radix_bits;
struct fb_params sorter_fb_params;

static struct cf_section sorter_config = {
  CF_ITEMS {
    CF_UNS("Trace", &sorter_trace),
    CF_UNS("PresortBuffer", &sorter_presort_bufsize),
    CF_UNS("StreamBuffer", &sorter_stream_bufsize),
    CF_SECTION("FileAccess", &sorter_fb_params, &fbpar_cf),
    CF_U64("SortBuffer", &sorter_bufsize),
    CF_UNS("Debug", &sorter_debug),
    CF_UNS("MinRadixBits", &sorter_min_radix_bits),
    CF_UNS("MaxRadixBits", &sorter_max_radix_bits),
    CF_END
  }
};

static void CONSTRUCTOR sorter_init_config(void)
{
  cf_declare_section("Sorter", &sorter_config, 0);
}
