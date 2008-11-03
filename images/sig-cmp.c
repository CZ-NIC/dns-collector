/*
 *	Image Library -- Comparisions of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "images/math.h"
#include "images/images.h"
#include "images/signature.h"

#include <stdio.h>

#define ASORT_PREFIX(x) image_signatures_dist_integrated_##x
#define ASORT_KEY_TYPE uns
#include "ucw/sorter/array-simple.h"

#define EXPLAIN
#include "images/sig-cmp-gen.h"
#include "images/sig-cmp-gen.h"
