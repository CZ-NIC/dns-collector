/*
 *	Sherlock Library -- Shell-Like Pattern Matching (currently only '?' and '*')
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <string.h>

#include "lib.h"

#define Convert(x) (x)
#define MATCH_FUNC_NAME match_pattern

#include "patmatch.h"
