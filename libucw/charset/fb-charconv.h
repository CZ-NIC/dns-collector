/*
 *	UCW Library -- Charset Conversion Wrapper for Fast Buffered I/O
 *
 *	(c) 2003--2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifdef CONFIG_UCW_CLEAN_ABI
#define fb_wrap_charconv_in ucw_fb_wrap_charconv_in
#define fb_wrap_charconv_out ucw_fb_wrap_charconv_out
#endif

struct fastbuf *fb_wrap_charconv_in(struct fastbuf *f, int cs_from, int cs_to);
struct fastbuf *fb_wrap_charconv_out(struct fastbuf *f, int cs_from, int cs_to);
