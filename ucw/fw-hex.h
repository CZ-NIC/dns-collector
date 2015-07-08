/*
 *	UCW Library -- I/O Wrapper for Hexadecimal Escaped Debugging Output
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_FW_HEX_H
#define _UCW_FW_HEX_H

#include <ucw/fastbuf.h>

#ifdef CONFIG_UCW_CLEAN_ABI
// FIXME
#endif

/***
 * When debugging a program, you might wonder what strange characters
 * are there in the output, or you might want to spice up the input
 * with Unicode snowmen to make the program freeze.
 *
 * In such situations, you can wrap your input or output stream in
 * the hex wrapper, which converts between strange characters and their
 * hexadecimal representation.
 ***/

/**
 * Creates an output hex wrapper for the given fastbuf. Printable ASCII
 * characters written to the wrapper are copied verbatim to @f.
 * Control characters, whitespace and everything outside ASCII
 * are transcribed hexadecimally as `<XY>`. A newline is appended
 * at the end of the output.
 **/
struct fastbuf *fb_wrap_hex_out(struct fastbuf *f);

/**
 * Creates an input hex wrapper for the given fastbuf. It reads characters
 * from @f and translates hexadecimal sequences `<XY>`. All other characters
 * are copied verbatim.
 **/
struct fastbuf *fb_wrap_hex_in(struct fastbuf *f);

#endif
