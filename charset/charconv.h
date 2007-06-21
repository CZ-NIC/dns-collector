/*
 *	Character Set Conversion Library 1.2
 *
 *	(c) 1998--2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _CHARSET_CHARCONV_H
#define _CHARSET_CHARCONV_H

struct conv_context {

  /* Parameters supplied by the caller */

  const unsigned char *source;		/* Current position in source buffer */
  const unsigned char *source_end;	/* End of source buffer */
  unsigned char *dest;			/* Current position in destination buffer */
  unsigned char *dest_start;		/* First byte of destination buffer */
  unsigned char *dest_end;		/* End of destination buffer */

  /* Internal variables */

  int (*convert)(struct conv_context *);
  int source_charset, dest_charset;
  unsigned short int *in_to_x;
  unsigned short int *x_to_out;
  unsigned int state, code, remains;
  unsigned char *string_at;
};

void conv_init(struct conv_context *);
void conv_set_charset(struct conv_context *, int, int);
#define conv_run(c) ((c)->convert(c))

#define CONV_SOURCE_END 1
#define CONV_DEST_END 2
#define CONV_SKIP 4

enum charset_id {
	CONV_CHARSET_ASCII,
	CONV_CHARSET_ISO_8859_1,
	CONV_CHARSET_ISO_8859_2,
	CONV_CHARSET_ISO_8859_3,
	CONV_CHARSET_ISO_8859_4,
	CONV_CHARSET_ISO_8859_5,
	CONV_CHARSET_ISO_8859_6,
	CONV_CHARSET_ISO_8859_7,
	CONV_CHARSET_ISO_8859_8,
	CONV_CHARSET_ISO_8859_9,
	CONV_CHARSET_ISO_8859_10,
	CONV_CHARSET_ISO_8859_11,
	CONV_CHARSET_ISO_8859_13,
	CONV_CHARSET_ISO_8859_14,
	CONV_CHARSET_ISO_8859_15,
	CONV_CHARSET_ISO_8859_16,
	CONV_CHARSET_WIN1250,
	CONV_CHARSET_WIN1251,
	CONV_CHARSET_WIN1252,
	CONV_CHARSET_KAMCS,
	CONV_CHARSET_CSN369103,
	CONV_CHARSET_CP852,
	CONV_CHARSET_MACCE,
	CONV_CHARSET_CORK,
	CONV_CHARSET_UTF8,
	CONV_NUM_CHARSETS
};

/* Conversion of a single character between current non-UTF8 charset and Unicode */
int conv_in_to_ucs(struct conv_context *c, unsigned int y);
int conv_ucs_to_out(struct conv_context *c, unsigned int ucs);

/* For those brave ones who want to mess with charconv internals */
unsigned int conv_x_to_ucs(unsigned int x);
unsigned int conv_ucs_to_x(unsigned int ucs);
unsigned int conv_x_count(void);

/* Charset names */

int find_charset_by_name(const char *);
char *charset_name(int);

#endif
