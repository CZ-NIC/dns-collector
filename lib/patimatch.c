/*
 *	Sherlock Library -- Shell-Like Case-Insensitive Pattern Matching (currently only '?' and '*')
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <string.h>

#include "lib.h"
#include "string.h"

#define Convert(x) Cupcase(x)
#define MATCH_FUNC_NAME match_pattern_nocase

#include "patmatch.h"
