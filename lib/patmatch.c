/*
 *	Sherlock Library -- Shell-Like Pattern Matching (currently only '?' and '*')
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#define Convert(x) (x)
#define MATCH_FUNC_NAME match_pattern

#include "lib/patmatch.h"
