/*
 *	A Program For Manipulation With Base224 Encoded Files
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

#include "ucw/lib.h"
#include "ucw/fastbuf.h"
#include "ucw/base224.h"

#include <stdio.h>

int main(int argc, char **argv)
{
  struct fastbuf *in = bfdopen_shared(0, 4096);
  struct fastbuf *out = bfdopen_shared(1, 4096);
  byte ib[BASE224_IN_CHUNK*10], ob[BASE224_OUT_CHUNK*10], *b;
  uns il, ol;

  if (argc != 2 || argv[1][0] != '-')
    goto usage;

  switch (argv[1][1])
    {
    case 'e':				/* Plain encoding */
      while (il = bread(in, ib, sizeof(ib)))
	{
	  ol = base224_encode(ob, ib, il);
	  bwrite(out, ob, ol);
	}
      break;
    case 'E':				/* Line block encoding */
      while (il = bread(in, ib, BASE224_IN_CHUNK*6))
	{
	  ol = base224_encode(ob, ib, il);
	  bputc(out, 'N');
	  bwrite(out, ob, ol);
	  bputc(out, '\n');
	}
      break;
    case 'd':				/* Plain decoding */
      while (ol = bread(in, ob, sizeof(ob)))
	{
	  il = base224_decode(ib, ob, ol);
	  bwrite(out, ib, il);
	}
      break;
    case 'D':				/* Line block decoding */
      while (b = bgets(in, ob, sizeof(ob)))
	{
	  if (!ob[0])
	    die("Invalid line syntax");
	  il = base224_decode(ib, ob+1, b-ob-1);
	  bwrite(out, ib, il);
	}
      break;
    default:
    usage:
      fputs("Usage: b224 (-e|-E|-d|-D)\n", stderr);
      return 1;
    }

  bclose(in);
  bclose(out);
  return 0;
}
