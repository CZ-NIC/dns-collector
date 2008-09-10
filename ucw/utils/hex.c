/*
 *  Hexadecimal dumper (CP/M style format)
 *
 *  Original version (c) Eric S. Raymond <esr@snark.thyrsus.com>
 *  Heavily modified by Martin Mares <mj@ucw.cz>
 *
 *  Can be freely distributed and used under the terms of the GNU GPL.
 */

#include "ucw/lib.h"
#include "ucw/lfs.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEFWIDTH 16		/* Default # chars to show per line */
#define MAXWIDTH 32		/* Maximum # of bytes per line	*/

typedef int bool;
#define TRUE	1
#define FALSE	0

static long	linesize = DEFWIDTH;	/* # of bytes to print per line */
static bool	cflag = FALSE;		/* show printables as ASCII if true */
static bool	gflag = FALSE;		/* suppress mid-page gutter if true */
static ucw_off_t	start = 0;		/* file offset to start dumping at */
static ucw_off_t	length = 0;		/* if nz, how many chars to dump */

static void dumpfile(FILE *f)
     /* dump a single, specified file -- stdin if filename is NULL */
{
  int     ch = '\0';		/* current character            */
  char    ascii[MAXWIDTH+3];	/* printable ascii data         */
  int     i = 0;		/* counter: # bytes processed	*/
  int     ai = 0;		/* index into ascii[]           */
  ucw_off_t offset = start;	/* byte offset of line in file  */
  int     hpos = 0;		/* horizontal position counter  */
  ucw_off_t fstart = start;
  ucw_off_t flength = length;
  char    *specials = "\b\f\n\r\t";
  char    *escapes = "bfnrt";
  char    *cp;

  if (fstart && ucw_seek(fileno(f), fstart, SEEK_SET) >= 0)
    fstart = 0;

  do {
    ch = getc(f);

    if (ch != EOF)
      {
	if (length && flength-- <= 0)
	  ch = EOF;
      }

    if (ch != EOF)
      {
	if (i++ % linesize == 0)
	  {
	    (void) printf("%04Lx ", (long long) offset);
	    offset += linesize;
	    hpos = 5;
	  }

	/* output one space for the mid-page gutter */
	if (!gflag)
	  if ((i - 1) % (linesize / 2) == 0)
	    {
	      (void) putchar(' ');
	      hpos++;
	      ascii[ai++] = ' ';
	    }

	/* dump the indicated representation of a character */
	ascii[ai] = (isprint (ch) || ch == ' ') ? ch : '.';

	if (cflag && (isprint(ch) || ch == ' '))
	  (void) printf("%c  ", ch);
	else if (cflag && ch && (cp = strchr(specials, ch)))
	  (void) printf("\\%c ", escapes[cp - specials]);
	else
	  (void) printf("%02x ", ch);

	/* update counters and things */
	ai++;
	hpos += 3;
      }

    /* At end-of-line or EOF, show ASCII version of data. */
    if (i && (ch == EOF || (i % linesize == 0)))
      {
	if (!cflag)
	  {
	    while (hpos < linesize * 3 + 7)
	      {
		hpos++;
		(void) putchar(' ');
	      }

	    ascii[ai] = '\0';
	    (void) printf("%s", ascii);
	  }

	if (ch != EOF || (i % linesize != 0))
	  (void) putchar('\n');
	ai = 0;		/* reset counters */
      }
  } while
    (ch != EOF);
}

static ucw_off_t getoffs(char *cp)
     /* fetch decimal or hex integer to be used as file start or offset */
{
  ucw_off_t value = 0;
  char *hexdigits = "0123456789abcdefABCDEF";

#if 0
  bool foundzero = FALSE;
  int base = 0;

  for (; *cp; cp++)
    if (*cp == '0')
      foundzero = TRUE;
    else if (isdigit(*cp))
      {
	base = 10;
	break;
      }
    else if (*cp = 'x' || *cp == 'X' || *cp == 'h' || *cp == 'H')
      {
	base = 16;
	cp++;
	break;
      }
    else
      return(-1L);

  if (base == 0)
    if (foundzero)
      base = 10;
    else
      return(-1L);

  if (base == 10)
    {
      for (; *cp; cp++)
	if (isdigit(*cp))
	  value = value * 10 + (*cp - '0');
	else
	  return(-1L);
    }
  else
#endif
    {
      for (; *cp; cp++)
	if (strchr(hexdigits, *cp))
	  value = value*16 + (strchr(hexdigits, tolower(*cp))-hexdigits);
	else
	  return -1;
    }

  return(value);
}

int main(int argc, char **argv)
{
  FILE    *infile;	    /* file pointer input file */
  int	    dumpcount = 0;  /* count of files dumped so far */
  char    *cp;
  int	  fd;

  for (argv++, argc--; argc > 0; argv++, argc--)
    {
      char s = **argv;

      if (s == '-' || s == '+')
	{
	  int	c = *++*argv;

	  switch (c)
	    {
	    case 'c': cflag = (s == '-'); continue;
	    case 'g': gflag = (s == '-'); continue;

	    case 's':
	      if ((*argv)[1])
		(*argv)++;
	      else
		argc--, argv++;
	      if (s == '-' && argc >= 0)
		{
		  if (cp = strchr(*argv, ','))
		    *cp++ = '\0';
		  if ((start = getoffs(*argv)) < 0)
		    {
		      (void) fputs("hex: start offset no good\n", stderr);
		      exit(1);
		    }

		  if (cp)
		    if ((length = getoffs(cp)) < 0)
		      {
			(void) fputs("hex: length no good\n", stderr);
			exit(1);
		      }
		}
	      else
		start = length = 0L;
	      continue;

	    case '\0':
	      infile = stdin;
	      break;

	    case 'w':
	      if ((*argv)[1])
		(*argv)++;
	      else
		argc--, argv++;
	      if ((linesize = getoffs(*argv)) == -1L || linesize > MAXWIDTH)
		{
		  (void) fputs("hex: line width no good\n", stderr);
		  exit(1);
		}
	      if (linesize % 2)
		gflag = TRUE;
	      continue;

	    default:
	      (void) fprintf(stderr, "hex: no such option as %s\n", *argv);
	      exit(1);
	    }
	}
      else
	{
	  fd = ucw_open(*argv, O_RDONLY, 0);
	  if (fd < 0 || !(infile = fdopen(fd, "r")))
	    {
	      (void) fprintf(stderr, "hex: cannot open %s: %m\n", *argv);
	      exit(1);
	    }
	}

      if (dumpcount > 0 || argc > 1)
	if (infile == stdin)
	  (void) printf("---- <Standard input> ----\n");
	else
	  (void) printf("---- %s ----\n", *argv);
      dumpfile(infile);
      dumpcount++;
      if (infile != stdin)
	(void) fclose(infile);
    }

  if (dumpcount == 0)
    dumpfile(stdin);
  return(0);
}
