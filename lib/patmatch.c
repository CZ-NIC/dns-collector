/*
 *	UCW Library -- Shell-Like Pattern Matching (currently only '?' and '*')
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#define Convert(x) (x)
#define MATCH_FUNC_NAME match_pattern

#include "lib/patmatch.h"
