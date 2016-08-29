/*
 *	UCW Library -- Hyper-super-meta-alt-control-shift extra fast
 *	str_len() and hash_*() routines
 *
 *	It is always at least as fast as the classical strlen() routine and for
 *	strings longer than 100 characters, it is substantially faster.
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/hashfunc.h>
#include <ucw/chartype.h>

/* The number of bits the hash in the function hash_*() is rotated by after
 * every pass.  It should be prime with the word size.  */
#define	SHIFT_BITS	7

/* A bit-mask which clears higher bytes than a given threshold.  */
static uint mask_higher_bits[sizeof(uint)];

static void CONSTRUCTOR
hashfunc_init(void)
{
	uint i, j;
	byte *str;
	for (i=0; i<sizeof(uint); i++)
	{
		str = (byte *) (mask_higher_bits + i);
		for (j=0; j<i; j++)
			str[j] = -1;
		for (j=i; j<sizeof(uint); j++)
			str[j] = 0;
	}
}

static inline uint CONST
str_len_uint(uint x)
{
	const uint sub = ~0U / 0xff;
	const uint and = sub * 0x80;
	uint a, i;
	byte *bytes;
	a = ~x & (x - sub) & and;
	/*
	 * x_2 = x - 0x01010101;
	 * x_3 = ~x & x_2;
	 * a = x_3 & 0x80808080;
	 *
	 * If all bytes of x are nonzero, then the highest bit of each byte of
	 * x_2 is lower or equal to the corresponding bit of x.  Hence x_3 has
	 * all these highest bits cleared (the target bit is set iff the source
	 * bit has changed from 0 to 1).  If a == 0, then we are sure there is
	 * no zero byte in x.
	 */
	if (!a)
		return sizeof(uint);
	bytes = (byte *) &x;
	for (i=0; i<sizeof(uint) && bytes[i]; i++);
	return i;
}

inline uint
str_len_aligned(const char *str)
{
	const uint *u = (const uint *) str;
	uint len = 0;
	while (1)
	{
		uint l = str_len_uint(*u++);
		len += l;
		if (l < sizeof(uint))
			return len;
	}
}

inline uint
hash_string_aligned(const char *str)
{
	const uint *u = (const uint *) str;
	uint hash = 0;
	while (1)
	{
		uint last_len = str_len_uint(*u);
		hash = ROL(hash, SHIFT_BITS);
		if (last_len < sizeof(uint))
		{
			uint tmp = *u & mask_higher_bits[last_len];
			hash ^= tmp;
			return hash;
		}
		hash ^= *u++;
	}
}

inline uint
hash_block_aligned(const byte *buf, uint len)
{
	const uint *u = (const uint *) buf;
	uint hash = 0;
	while (len >= sizeof(uint))
	{
		hash = ROL(hash, SHIFT_BITS) ^ *u++;
		len -= sizeof(uint);
	}
	hash = ROL(hash, SHIFT_BITS) ^ (*u & mask_higher_bits[len]);
	return hash;
}

#ifndef	CPU_ALLOW_UNALIGNED
uint
str_len(const char *str)
{
	uint shift = UNALIGNED_PART(str, uint);
	if (!shift)
		return str_len_aligned(str);
	else
	{
		uint i;
		shift = sizeof(uint) - shift;
		for (i=0; i<shift; i++)
			if (!str[i])
				return i;
		return shift + str_len_aligned(str + shift);
	}
}

uint
hash_string(const char *str)
{
	const byte *s = str;
	uint shift = UNALIGNED_PART(s, uint);
	if (!shift)
		return hash_string_aligned(s);
	else
	{
		uint hash = 0;
		uint i;
		for (i=0; ; i++)
		{
			uint modulo = i % sizeof(uint);
			uint shift;
#ifdef	CPU_LITTLE_ENDIAN
			shift = modulo;
#else
			shift = sizeof(uint) - 1 - modulo;
#endif
			if (!modulo)
				hash = ROL(hash, SHIFT_BITS);
			if (!s[i])
				break;
			hash ^= s[i] << (shift * 8);
		}
		return hash;
	}
}

uint
hash_block(const byte *buf, uint len)
{
	uint shift = UNALIGNED_PART(buf, uint);
	if (!shift)
		return hash_block_aligned(buf, len);
	else
	{
		uint hash = 0;
		uint i;
		for (i=0; ; i++)
		{
			uint modulo = i % sizeof(uint);
			uint shift;
#ifdef	CPU_LITTLE_ENDIAN
			shift = modulo;
#else
			shift = sizeof(uint) - 1 - modulo;
#endif
			if (!modulo)
				hash = ROL(hash, SHIFT_BITS);
			if (i >= len)
				break;
			hash ^= buf[i] << (shift * 8);
		}
		return hash;
	}
}
#endif

uint
hash_string_nocase(const char *str)
{
	const byte *s = str;
	uint hash = 0;
	uint i;
	for (i=0; ; i++)
	{
		uint modulo = i % sizeof(uint);
		uint shift;
#ifdef	CPU_LITTLE_ENDIAN
		shift = modulo;
#else
		shift = sizeof(uint) - 1 - modulo;
#endif
		if (!modulo)
			hash = ROL(hash, SHIFT_BITS);
		if (!s[i])
			break;
		hash ^= Cupcase(s[i]) << (shift * 8);
	}
	return hash;
}
