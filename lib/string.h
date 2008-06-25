/*
 *	UCW Library -- String Routines
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *	(c) 2007--2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_STRING_H
#define _UCW_STRING_H

/* string.c */

char *str_format_flags(char *dest, const char *fmt, uns flags);

/* str-esc.c */

char *str_unesc(char *dest, const char *src);

/* str-split.c */

int str_sepsplit(char *str, uns sep, char **rec, uns max);
int str_wordsplit(char *str, char **rec, uns max);

/* str-(i)match.c: Matching of shell patterns */

int str_match_pattern(const char *patt, const char *str);
int str_match_pattern_nocase(const char *patt, const char *str);

/* str-hex.c */

void mem_to_hex(char *dest, const byte *src, uns bytes, uns sep);
const char *hex_to_mem(byte *dest, const char *src, uns max_bytes, uns sep);

/* md5hex.c */

void md5_to_hex(const byte *s, char *d);
void hex_to_md5(const char *s, byte *d);

#endif
