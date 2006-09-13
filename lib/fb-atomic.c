/*
 *	UCW Library -- Atomic Buffered Write to Files
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *	This fastbuf backend is intended for cases where several threads
 *	of a single program append records to a single file and while the
 *	record can mix in an arbitrary way, the bytes inside a single
 *	record must remain uninterrupted.
 *
 *	In case of files with fixed record size, we just allocate the
 *	buffer to hold a whole number of records and take advantage
 *	of the atomicity of the write() system call.
 *
 *	With variable-sized records, we need another solution: when
 *	writing a record, we keep the fastbuf in a locked state, which
 *	prevents buffer flushing (and if the buffer becomes full, we extend it),
 *	and we wait for an explicit commit operation which write()s the buffer
 *	if the free space in the buffer falls below the expected maximum record
 *	length.
 *
 *	fbatomic_create() is called with the following parameters:
 *	    name - name of the file to open
 *	    master - fbatomic for the master thread or NULL if it's the first open
 *	    bufsize - initial buffer size
 *	    record_len - record length for fixed-size records;
 *		or -(expected maximum record length) for variable-sized ones.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

struct fb_atomic {
  struct fastbuf fb;
  int fd;				/* File descriptor */
  uns locked;
  uns expected_max_len;
  byte *expected_max_bptr;
};
#define FB_ATOMIC(f) ((struct fb_atomic *)(f)->is_fastbuf)
