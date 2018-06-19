/*
 *	UCW Library -- Universal Sorter: Configuration
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/fastbuf.h>
#include <ucw/sorter/common.h>

uint sorter_trace;
uint sorter_trace_array;
u64 sorter_bufsize = 65536;
uint sorter_debug;
uint sorter_min_radix_bits;
uint sorter_max_radix_bits;
uint sorter_add_radix_bits;
uint sorter_min_multiway_bits;
uint sorter_max_multiway_bits;
uint sorter_threads;
u64 sorter_thread_threshold = 1048576;
u64 sorter_thread_chunk = 4096;
u64 sorter_radix_threshold = 4096;
struct fb_params sorter_fb_params;
struct fb_params sorter_small_fb_params;
u64 sorter_small_input;

static struct cf_section sorter_config = {
  CF_ITEMS {
    CF_UINT("Trace", &sorter_trace),
    CF_UINT("TraceArray", &sorter_trace_array),
    CF_SECTION("FileAccess", &sorter_fb_params, &fbpar_cf),
    CF_SECTION("SmallFileAccess", &sorter_fb_params, &fbpar_cf),
    CF_U64("SmallInput", &sorter_small_input),
    CF_U64("SortBuffer", &sorter_bufsize),
    CF_UINT("Debug", &sorter_debug),
    CF_UINT("MinRadixBits", &sorter_min_radix_bits),
    CF_UINT("MaxRadixBits", &sorter_max_radix_bits),
    CF_UINT("AddRadixBits", &sorter_add_radix_bits),
    CF_UINT("MinMultiwayBits", &sorter_min_multiway_bits),
    CF_UINT("MaxMultiwayBits", &sorter_max_multiway_bits),
    CF_UINT("Threads", &sorter_threads),
    CF_U64("ThreadThreshold", &sorter_thread_threshold),
    CF_U64("ThreadChunk", &sorter_thread_chunk),
    CF_U64("RadixThreshold", &sorter_radix_threshold),
    CF_END
  }
};

static void CONSTRUCTOR sorter_init_config(void)
{
  cf_declare_section("Sorter", &sorter_config, 0);
}
