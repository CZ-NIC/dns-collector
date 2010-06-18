/*
 *	UCW Library -- Conversions of Strings to Numbers: Declarations
 *
 *	(c) 2010 Daniel Fiala <danfiala@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _STRTONUM_H
#define _STRTONUM_H

enum str_to_num_flags {
  STN_SIGNED = 0x20,       // The resulting range is signed
  STN_MINUS = 0x40,        // Allow optional '-' sign
  STN_PLUS = 0x80,         // Allow optional '+' sign
  STN_TRUNC = 0x100,       // Allow range overflow -> truncate to the resulting range
  STN_DEC = 0x200,         // Support decimal numbers
  STN_HEX = 0x400,         // Support hexadecimal numbers (0x...)
  STN_BIN = 0x800,         // Support binary numbers (0b...)
  STN_OCT = 0x1000,        // Support octal numbers (0o...)
  STN_UNDERSCORE = 0x2000, // Number can contain underscore characters to increase readability (eg. 1_000_000)
  STN_ZCHAR = 0x4000,      // Number can be terminated only by \0 character
};

#define STN_DBASES_MASK    0x1F
#define STN_SIGNS          (STN_MINUS | STN_PLUS)
#define STN_BASES          (STN_DEC | STN_HEX | STN_BIN | STN_OCT)
#define STN_FLAGS      (STN_MINUS | STN_PLUS | STN_BASES)
#define STN_UFLAGS     (STN_FLAGS | STN_UNDERSCORE)
#define STN_SFLAGS     (STN_FLAGS | STN_SIGNED)
#define STN_USFLAGS    (STN_SFLAGS | STN_UNDERSCORE)

#define STN_DECLARE_CONVERTOR(type, suffix)                                                         \
const char *str_to_##suffix(type *num, const char *str, const char **next, const uns flags)

#define STN_SIGNED_CONVERTOR(type, suffix, usuffix)                                                  \
static inline const char *str_to_##suffix(type *num, const char *str, const char **next, const uns flags) \
{                                                                                                   \
  return str_to_##usuffix((void *) num, str, next, flags | STN_SIGNED);                               \
}

STN_DECLARE_CONVERTOR(uns, uns);
STN_SIGNED_CONVERTOR(int, int, uns)

STN_DECLARE_CONVERTOR(uintmax_t, uintmax);
STN_SIGNED_CONVERTOR(intmax_t, intmax, uintmax)

#endif
