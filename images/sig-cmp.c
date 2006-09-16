/*
 *	Image Library -- Comparitions of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/math.h"
#include "images/math.h"
#include "images/images.h"
#include "images/signature.h"

#include <stdio.h>

#define ASORT_PREFIX(x) image_signatures_dist_integrated_##x
#define ASORT_KEY_TYPE uns
#define ASORT_ELT(i) items[i]
#define ASORT_EXTRA_ARGS , uns *items
#include "lib/arraysort.h"

#define EXPLAIN
#include "images/sig-cmp-gen.h"
#include "images/sig-cmp-gen.h"

uns
image_signatures_dist(struct image_signature *sig1, struct image_signature *sig2)
{
  switch (image_sig_compare_method)
    {
      case 0:
	return image_signatures_dist_integrated(sig1, sig2);
      case 1:
	return image_signatures_dist_fuzzy(sig1, sig2);
      case 2:
	return image_signatures_dist_average(sig1, sig2);
      default:
	ASSERT(0);
    }
}

uns
image_signatures_dist_explain(struct image_signature *sig1, struct image_signature *sig2, void (*msg)(byte *text, void *param), void *param)
{
  switch (image_sig_compare_method)
    {
      case 0:
	return image_signatures_dist_integrated_explain(sig1, sig2, msg, param);
      case 1:
	return image_signatures_dist_fuzzy_explain(sig1, sig2, msg, param);
      case 2:
	return image_signatures_dist_average_explain(sig1, sig2, msg, param);
      default:
	ASSERT(0);
    }
}

