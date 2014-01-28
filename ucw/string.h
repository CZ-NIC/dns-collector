/*
 *	UCW Library -- String Routines
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *	(c) 2007--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_STRING_H
#define _UCW_STRING_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define hex_to_mem ucw_hex_to_mem
#define mem_to_hex ucw_mem_to_hex
#define str_count_char ucw_str_count_char
#define str_format_flags ucw_str_format_flags
#define str_has_prefix ucw_str_has_prefix
#define str_has_suffix ucw_str_has_suffix
#define str_hier_prefix ucw_str_hier_prefix
#define str_hier_suffix ucw_str_hier_suffix
#define str_match_pattern ucw_str_match_pattern
#define str_match_pattern_nocase ucw_str_match_pattern_nocase
#define str_sepsplit ucw_str_sepsplit
#define str_unesc ucw_str_unesc
#define str_wordsplit ucw_str_wordsplit
#endif

/* string.c */

#ifdef CONFIG_DARWIN
uns strnlen(const char *str, uns n);	// NOAPI
#endif

/**
 * Format a set of flag bits. When the i-th bit of @flags is 1,
 * set the i-th character of @dest to @fmt[i], otherwise to '-'.
 **/
char *str_format_flags(char *dest, const char *fmt, uns flags);

/** Counts occurrences of @chr in @str. **/
uns str_count_char(const char *str, uns chr);

/* str-esc.c */

/**
 * Decode a string with backslash escape sequences as in C99 strings.
 * It is safe to pass @dest equal to @src.
 **/
char *str_unesc(char *dest, const char *src);

/* str-split.c */

/**
 * Split @str to at most @max fields separated by @sep. Occurrences of the
 * separator are rewritten to string terminators, @rec[i] is set to point
 * to the i-th field. The total number of fields is returned.
 *
 * When there are more than @max fields in @str, the first @max fields
 * are processed and -1 is returned.
 **/
int str_sepsplit(char *str, uns sep, char **rec, uns max);

/**
 * Split @str to words separated by white-space characters. The spaces
 * are replaced by string terminators, @rec[i] is set to point to the
 * i-th field. The total number of fields is returned.
 *
 * When there are more than @max fields in @str, the first @max fields
 * are processed and -1 is returned.
 *
 * Fields surrounded by double quotes are also recognized. They can contain
 * spaces, but no mechanism for escaping embedded quotes is defined.
 **/
int str_wordsplit(char *str, char **rec, uns max);

/* str-(i)match.c: Matching of shell patterns */

/**
 * Test whether the string @str matches the shell-like pattern @patt. Only
 * "*" and "?" meta-characters are supported.
 **/
int str_match_pattern(const char *patt, const char *str);

/** A case-insensitive version of @str_match_pattern(). **/
int str_match_pattern_nocase(const char *patt, const char *str);

/* str-hex.c */

/**
 * Create a hexdump of a memory block of @bytes bytes starting at @src.
 * The @flags contain an optional separator of bytes (0 if bytes should
 * not be separated), possibly OR-ed with `MEM_TO_HEX_UPCASE` when upper-case
 * characters should be used.
 **/
void mem_to_hex(char *dest, const byte *src, uns bytes, uns flags);

/**
 * An inverse function to @mem_to_hex(). Takes a hexdump of at most @max_bytes
 * bytes and stores the bytes to a buffer starting at @dest. Returns a pointer
 * at the first character after the dump.
 **/
const char *hex_to_mem(byte *dest, const char *src, uns max_bytes, uns flags);

// Bottom 8 bits of flags are an optional separator of bytes, the rest is:
#define MEM_TO_HEX_UPCASE 0x100

/* str-fix.c */

int str_has_prefix(const char *str, const char *prefix); /** Tests if @str starts with @prefix. **/
int str_has_suffix(const char *str, const char *suffix); /** Tests if @str ends with @suffix. **/

/**
 * Let @str and @prefix be hierarchical names with components separated by
 * a character @sep. Returns true if @str starts with @prefix, respecting
 * component boundaries.
 *
 * For example, when @sep is '/' and @str is "/usr/local/bin", then:
 *
 * - "/usr/local" is a prefix
 * - "/usr/local/" is a prefix, too
 * - "/usr/loc" is not,
 * - "/usr/local/bin" is a prefix,
 * - "/usr/local/bin/" is not,
 * - "/" is a prefix,
 * - "" is a prefix.
 **/
int str_hier_prefix(const char *str, const char *prefix, uns sep);
int str_hier_suffix(const char *str, const char *suffix, uns sep); /** Like @str_hier_prefix(), but for suffixes. **/

#endif
