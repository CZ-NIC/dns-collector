/*
 *	The UniCode Library -- Debugging Support Functions
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "charset/unicode.h"

static byte *
get_static_buffer(uns size)
{
  static byte *static_debug_buffer;
  static uns static_debug_size;

  if (!static_debug_buffer)
    {
      if (size < 1024)
	size = 1024;
      static_debug_buffer = xmalloc(size);
      static_debug_size = size;
    }
  else if (static_debug_size < size)
    {
      size = (size+1023) & ~1023;
      static_debug_buffer = xrealloc(static_debug_buffer, size);
      static_debug_size = size;
    }
  return static_debug_buffer;
}

byte *
static_ucs2_to_utf8(word *w)
{
  byte *buf = get_static_buffer(Ustrlen(w) * 3 + 1);

  ucs2_to_utf8(buf, w);
  return buf;
}
