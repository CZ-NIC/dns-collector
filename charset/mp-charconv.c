/*
 *	Sherlock Library -- Character Conversion with Allocation on a Memory Pool 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/mempool.h"
#include "charset/mp-charconv.h"
#include "charset/stk-charconv.h"
#include <string.h>

byte *
mp_conv(struct mempool *mp, byte *s, uns in_cs, uns out_cs)
{
  return mp_strdup(mp, stk_conv(s, in_cs, out_cs));
}
