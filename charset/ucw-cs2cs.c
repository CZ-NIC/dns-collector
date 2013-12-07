/*
 *	Simple character set convertor
 *
 *	(c) 1998 Pavel Machek <pavel@ucw.cz>
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License.
 */

#include <ucw/lib.h>
#include <charset/charconv.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifdef TEST
#define BUFSIZE 13
#else
#define BUFSIZE 4096
#endif

int
main(int argc, char **argv)
{
  struct conv_context ctxt;
  int ch_from, ch_to, n, flags;
  char inbuf[BUFSIZE], outbuf[BUFSIZE];

  if (argc != 3)
    die("ucw-cs2cs in-charset out-charset");
  conv_init(&ctxt);
  ch_from = find_charset_by_name(argv[1]);
  if (ch_from < 0)
    die("Unknown charset %s", argv[1]);
  ch_to = find_charset_by_name(argv[2]);
  if (ch_to < 0)
    die("Unknown charset %s", argv[2]);

  conv_set_charset(&ctxt, ch_from, ch_to);
  while ((n = read(0, inbuf, sizeof(inbuf))) > 0)
    {
      ctxt.source = inbuf;
      ctxt.source_end = inbuf + n;
      ctxt.dest = ctxt.dest_start = outbuf;
      ctxt.dest_end = outbuf + sizeof(outbuf);
      do
	{
	  flags = conv_run(&ctxt);
	  if (flags & (CONV_SOURCE_END | CONV_DEST_END))
	    {
	      int w = write(1, ctxt.dest_start, ctxt.dest - ctxt.dest_start);
	      if (w < 0)
		die("write error: %m");
	      ctxt.dest = outbuf;
	    }
	}
      while (! (flags & CONV_SOURCE_END));
    }
  if (n < 0)
    die("read error: %m");
  return 0;
}
