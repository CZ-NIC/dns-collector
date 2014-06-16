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

#ifdef CONFIG_UCW_CLEAN_ABI
#define str_to_uint ucw_str_to_uint
#define str_to_uintmax ucw_str_to_uintmax
#define str_to_uns ucw_str_to_uns
#endif

// Set (flags & 0x1f) in the range 1 to 31 to denote the default base of the number
enum str_to_num_flags {
  STN_SIGNED = 0x20,       // The resulting range is signed
  STN_MINUS = 0x40,        // Allow optional '-' sign
  STN_PLUS = 0x80,         // Allow optional '+' sign
  STN_TRUNC = 0x100,       // Allow range overflow -> truncate to the allowed range
  STN_DEC = 0x200,         // Support decimal numbers (currently no prefix)
  STN_HEX = 0x400,         // Support hexadecimal numbers (0x...)
  STN_BIN = 0x800,         // Support binary numbers (0b...)
  STN_OCT = 0x1000,        // Support octal numbers (0o...)
  STN_OCT0 = 0x2000,       // Support octal numbers (0[0-7]...)
  STN_UNDERSCORE = 0x4000, // Number can contain underscore characters to increase readability (eg. 1_000_000)
  STN_WHOLE = 0x8000,      // Number can be terminated only by \0 character
};

#define STN_DBASES_MASK    0x1F
#define STN_SIGNS          (STN_MINUS | STN_PLUS)
#define STN_BASES          (STN_DEC | STN_HEX | STN_BIN | STN_OCT)
#define STN_BASES0         (STN_BASES | STN_OCT0)
#define STN_FLAGS          (STN_MINUS | STN_PLUS | STN_BASES)
#define STN_UFLAGS         (STN_FLAGS | STN_UNDERSCORE)
#define STN_SFLAGS         (STN_FLAGS | STN_SIGNED)
#define STN_USFLAGS        (STN_SFLAGS | STN_UNDERSCORE)

#define STN_DECLARE_CONVERTOR(type, suffix)                                                               \
const char *str_to_##suffix(type *num, const char *str, const char **next, const uint flags)

#define STN_SIGNED_CONVERTOR(type, suffix, usuffix)                                                       \
static inline const char *str_to_##suffix(type *num, const char *str, const char **next, const uint flags) \
{                                                                                                         \
  return str_to_##usuffix((void *) num, str, next, flags | STN_SIGNED | STN_PLUS | STN_MINUS);            \
}

STN_DECLARE_CONVERTOR(uint, uint);
STN_SIGNED_CONVERTOR(int, int, uint)

STN_DECLARE_CONVERTOR(uintmax_t, uintmax);
STN_SIGNED_CONVERTOR(intmax_t, intmax, uintmax)

// FIXME: For backward compatibility, will be removed soon
STN_DECLARE_CONVERTOR(uns, uns);

#endif
