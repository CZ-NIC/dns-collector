/*
 *	Character Set Conversion Library 1.1
 *
 *	(c) 1998--2001 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License. See file COPYING in any of the GNU packages.
 */

struct conv_context {

  /* Parameters supplied by the caller */

  const unsigned char *source;		/* Current position in source buffer */
  const unsigned char *source_end;	/* End of source buffer */
  unsigned char *dest;			/* Current position in destination buffer */
  unsigned char *dest_start;		/* First byte of destination buffer */
  unsigned char *dest_end;		/* End of destination buffer */

  /* Internal variables */

  int (*convert)(struct conv_context *);
  unsigned short int *in_to_x;
  unsigned short int *x_to_out;
  unsigned int state, value;
};

void conv_init(struct conv_context *);
void conv_set_charset(struct conv_context *, int, int);
#define conv_run(c) ((c)->convert(c))

#define CONV_SOURCE_END 1
#define CONV_DEST_END 2
#define CONV_SKIP 4

#define CONV_CHARSET_ASCII 0
#define CONV_CHARSET_LATIN1 1
#define CONV_CHARSET_LATIN2 2
#define CONV_CHARSET_UTF8 8
#define CONV_NUM_CHARSETS 9

/* For those brave ones who want to mess with charconv internals */
unsigned int conv_x_to_ucs(unsigned int x);
unsigned int conv_ucs_to_x(unsigned int ucs);
unsigned int conv_x_count(void);

/* Charset names */

int find_charset_by_name(char *);
char *charset_name(int);
