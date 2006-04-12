/*
 *	Image Library -- Thumbnails
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _IMAGE_THUMB_H
#define _IMAGE_THUMB_H

struct odes;
struct mempool;
struct image;

int decompress_thumbnail(struct odes *obj, struct mempool *pool, struct image *image);

#endif
