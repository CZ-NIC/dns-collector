/*
 *	Sherlock Library -- Shell-Like Case-Insensitive Pattern Matching (currently only '?' and '*')
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/chartype.h"

#define Convert(x) Cupcase(x)
#define MATCH_FUNC_NAME match_pattern_nocase

#include "lib/patmatch.h"
