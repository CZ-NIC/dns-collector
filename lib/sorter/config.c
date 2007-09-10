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
uns sorter_stream_bufsize = 65536;
u64 sorter_bufsize = 65536;
uns sorter_debug;
uns sorter_min_radix_bits;
uns sorter_max_radix_bits;
uns sorter_min_multiway_bits;
uns sorter_max_multiway_bits;
uns sorter_threads;
uns sorter_thread_threshold = 1048576;
uns sorter_thread_chunk = 4096;
uns sorter_radix_threshold = 4096;
struct fb_params sorter_fb_params;

static struct cf_section sorter_config = {
  CF_ITEMS {
    CF_UNS("Trace", &sorter_trace),
    CF_UNS("StreamBuffer", &sorter_stream_bufsize),
    CF_SECTION("FileAccess", &sorter_fb_params, &fbpar_cf),
    CF_U64("SortBuffer", &sorter_bufsize),
    CF_UNS("Debug", &sorter_debug),
    CF_UNS("MinRadixBits", &sorter_min_radix_bits),
    CF_UNS("MaxRadixBits", &sorter_max_radix_bits),
    CF_UNS("MinMultiwayBits", &sorter_min_multiway_bits),
    CF_UNS("MaxMultiwayBits", &sorter_max_multiway_bits),
    CF_UNS("Threads", &sorter_threads),
    CF_UNS("ThreadThreshold", &sorter_thread_threshold),
    CF_UNS("ThreadChunk", &sorter_thread_chunk),
    CF_UNS("RadixThreshold", &sorter_radix_threshold),
    CF_END
  }
};

static void CONSTRUCTOR sorter_init_config(void)
{
  cf_declare_section("Sorter", &sorter_config, 0);
}
