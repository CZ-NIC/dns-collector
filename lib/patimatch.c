/*
 *	Sherlock Library -- Shell-Like Case-Insensitive Pattern Matching (currently only '?' and '*')
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/chartype.h"

#define Convert(x) Cupcase(x)
#define MATCH_FUNC_NAME match_pattern_nocase

#include "lib/patmatch.h"
