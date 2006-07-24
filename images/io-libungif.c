/*
 *	Image Library -- libungif
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define LOCAL_DEBUG

#include "lib/lib.h"
#include "images/images.h"

int
libungif_read_header(struct image_io *io)
{
  image_thread_err(io->thread, IMAGE_ERR_NOT_IMPLEMENTED, "Libungif read not implemented.");
  return 0;
}

int
libungif_read_data(struct image_io *io UNUSED)
{
  ASSERT(0);
}
