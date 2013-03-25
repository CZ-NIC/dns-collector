/*
 *	UCW Library -- Coding of u64 into variable length bytecode.
 *
 *	(c) 2013 Tomas Valla <tom@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_VARINT_H
#define _UCW_VARINT_H

/***
 * The encoding works in the following way:
 *
 * First byte			Stored values
 *
 * 0xxxxxxx			7 bits
 * 10xxxxxx  +1 byte		14 bits, value shifted by 2^7
 * 110xxxxx  +2 bytes		21 bits, value shifted by 2^7+2^14
 * 1110xxxx  +3 bytes		28 bits, value shifted by 2^7+2^14+2^21
 *    ....
 * 11111110  +7 bytes		56 bits, value shifted by 2^7+2^14+2^21+2^28+2^35+2^42
 * 11111111  +8 bytes		full 64 bits, value shifted by 2^7+2^14+2^21+2^28+2^35+2^42+2^56
 *
 * The values are stored in bigendian to allow lexicographic sorting.
 * The encoding and algorithms are aimed to be optimised for storing shorter numbers.
 *
 * There is an invalid combination: if the first two bytes are 0xff 0xff.
 ***/


#define VARINT_SHIFT_L1			      0x80
#define VARINT_SHIFT_L2			    0x4080
#define VARINT_SHIFT_L3			  0x204080
#define VARINT_SHIFT_L4			0x10204080
#define VARINT_SHIFT_L5		      0x0810204080
#define VARINT_SHIFT_L6		    0x040810204080
#define VARINT_SHIFT_L7		  0x02040810204080
#define VARINT_SHIFT_L8		0x0102040810204080

/*
 * The following code is already unrolled, to maximize the speed.
 * Yes, it is ugly.
 */

#define PUTB(j,i)  p[j] = (byte)((u >> (8*(i))));
#define PUTB4(b)   PUTB(0,b-1) PUTB(1,b-2) PUTB(2,b-3) PUTB(3,b-4)

/* for internal use only, need the length > 4 */
uns varint_put_big(byte *p, u64 u);
const byte *varint_get_big(const byte *p, u64 *r);

/**
 * Encode u64 value u into byte sequence p.
 * Returns the number of bytes used (at least 1 and at most 9).
 **/
static inline uns varint_put(byte *p, u64 u)
{
	if (u < VARINT_SHIFT_L1) {
		p[0] = (byte)u;
		return 1;
	}
	if (u < VARINT_SHIFT_L2) {
		u -= VARINT_SHIFT_L1;
		PUTB(0,1)
		p[0] |= 0x80;
		PUTB(1,0)
		return 2;
	}
	if (u < VARINT_SHIFT_L3) {
		u -= VARINT_SHIFT_L2;
		PUTB(0,2)
		p[0] |= 0xc0;
		PUTB(1,1) PUTB(2,0)
		return 3;
	}
	if (u < VARINT_SHIFT_L4) {
		u -= VARINT_SHIFT_L3;
		PUTB4(4)
		p[0] |= 0xe0;
		return 4;
	}
	return varint_put_big(p, u);
}
#undef GETB
#undef GETB4

/**
 * Decode u64 value from a byte sequence p into res.
 * The invalid combination is not detected.
 * Returns pointer to the next position after the sequence.
 **/
static inline const byte *varint_get(const byte *p, u64 *res)
{
	byte h = ~*p;

	if (h & 0x80) {
		*res = p[0];
		return p+1;
	}
	if (h & 0x40) {
		*res = (p[0] & 0x3f)<<8 | p[1];
		*res += VARINT_SHIFT_L1;
		return p+2;
	}
	if (h & 0x20) {
		*res = (p[0] & 0x1f)<<16 | p[1]<<8 | p[2];
		*res += VARINT_SHIFT_L2;
		return p+3;
	}
	if (h & 0x10) {
		*res = (p[0] & 0xf)<<24 | p[1]<<16 | p[2]<<8 | p[3];
		*res += VARINT_SHIFT_L3;
		return p+4;
	}
	return varint_get_big(p, res);
}

/**
 * Decode at most u32 value from a byte sequence p into u32 res.
 * The invalid combination is not detected.
 * Returns pointer to the next position after the sequence.
 * If the stored number cannot fit into u32, fill res by 0xffffffff and returns p.
 **/
static inline const byte *varint_get32(const byte *p, u32 *res)
{
	byte h = ~*p;

	if (h & 0x80) {
		*res = p[0];
		return p+1;
	}
	if (h & 0x40) {
		*res = (p[0] & 0x3f)<<8 | p[1];
		*res += VARINT_SHIFT_L1;
		return p+2;
	}
	if (h & 0x20) {
		*res = (p[0] & 0x1f)<<16 | p[1]<<8 | p[2];
		*res += VARINT_SHIFT_L2;
		return p+3;
	}
	if (h & 0x10) {
		*res = (p[0] & 0xf)<<24 | p[1]<<16 | p[2]<<8 | p[3];
		*res += VARINT_SHIFT_L3;
		return p+4;
	}
	u64 r = (u64)(p[0] & 7)<<32 | (u64)p[1]<<24 | (u64)p[2]<<16 | (u64)p[3]<<8 | (u64)p[4];
	r += VARINT_SHIFT_L4;
	if (r > 0xffffffff) {
		*res = 0xffffffff;
		return p;
	}
	*res = r;
	return p+5;
}

/** Does the byte sequence p code the invalid sequence? **/
static inline int varint_invalid(const byte *p)
{
	return p[0] == 0xff && p[1] == 0xff;
}

/**
 * Store the invalid sequence.
 * Returns always 2 (2 bytes were used, to be consistent with varint_put).
 **/
static inline uns varint_put_invalid(byte *p)
{
	p[0] = p[1] = 0xff;
	return 2;
}

/** Compute the length of encoding in bytes from the first byte hdr of the encoding. **/
static inline uns varint_len(const byte hdr)
{
	byte b = ~hdr;

	uns l = 0;
	if (!b)
		l = -1;
	else {
		if (b & 0xf0) { l += 4; b &= 0xf0; }
		if (b & 0xcc) { l += 2; b &= 0xcc; }
		if (b & 0xaa) l++;
	}
	return 8 - l;
}

/** Compute the number of bytes needed to store the value u. **/
static inline uns varint_space(u64 u)
{
	if (u < VARINT_SHIFT_L1)
		return 1;
	if (u < VARINT_SHIFT_L2)
		return 2;
	if (u < VARINT_SHIFT_L3)
		return 3;
	if (u < VARINT_SHIFT_L4)
		return 4;
	if (u < VARINT_SHIFT_L5)
		return 5;
	if (u < VARINT_SHIFT_L6)
		return 6;
	if (u < VARINT_SHIFT_L7)
		return 7;
	if (u < VARINT_SHIFT_L8)
		return 8;
	return 9;
}

#endif
