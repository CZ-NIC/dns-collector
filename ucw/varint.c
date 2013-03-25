/*
 *	UCW Library -- Coding of u64 into variable length bytecode.
 *
 *	(c) 2013 Tomas Valla <tom@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/varint.h>

#define PUTB(j,i)  p[j] = (byte)((u >> (8*(i))));
#define PUTB4(b)   PUTB(0,b-1) PUTB(1,b-2) PUTB(2,b-3) PUTB(3,b-4)
uns varint_put_big(byte *p, u64 u)
{
	ASSERT(u >= VARINT_SHIFT_L4);

	if (u < VARINT_SHIFT_L5) {
		u -= VARINT_SHIFT_L4;
		PUTB4(5)
		p[0] |= 0xf0;
		PUTB(4,0);
		return 5;
	}
	if (u < VARINT_SHIFT_L6) {
		u -= VARINT_SHIFT_L5;
		PUTB4(6)
		PUTB(4,1) PUTB(5,0)
		p[0] |= 0xf8;
		return 6;
	}
	if (u < VARINT_SHIFT_L7) {
		u -= VARINT_SHIFT_L6;
		PUTB4(7)
		p[0] |= 0xfc;
		PUTB(4,2) PUTB(5,1) PUTB(6,0)
		return 7;
	}
	if (u < VARINT_SHIFT_L8) {
		u -= VARINT_SHIFT_L7;
		p[0] = 0xfe;
		PUTB(1,6) PUTB(2,5) PUTB(3,4) PUTB(4,3)
		PUTB(5,2) PUTB(6,1) PUTB(7,0)
		return 8;
	}
	u -= VARINT_SHIFT_L8;
	p[0] = 0xff;
	PUTB(1,7) PUTB(2,6) PUTB(3,5) PUTB(4,4)
	PUTB(5,3) PUTB(6,2) PUTB(7,1) PUTB(8,0)
	return 9;
}

const byte *varint_get_big(const byte *p, u64 *r)
{
	ASSERT((*p & 0xf0) == 0xf0);

	byte h = ~*p;
	if (h & 0x08) {
		*r = (u64)(p[0] & 7)<<32 | (u64)p[1]<<24 | (u64)p[2]<<16 | (u64)p[3]<<8 | (u64)p[4];
		*r += VARINT_SHIFT_L4;
		return p+5;
	}
	if (h & 0x04) {
		*r = (u64)(p[0] & 3)<<40 | (u64)p[1]<<32 | (u64)p[2]<<24 | (u64)p[3]<<16 | (u64)p[4]<<8 | (u64)p[5];
		*r += VARINT_SHIFT_L5;
		return p+6;
	}
	if (h & 0x02) {
		*r = (u64)(p[0] & 1)<<48 | (u64)p[1]<<40 | (u64)p[2]<<32 | (u64)p[3]<<24 | (u64)p[4]<<16 | (u64)p[5]<<8 | (u64)p[6];
		*r += VARINT_SHIFT_L6;
		return p+7;
	}
	if (h & 0x01) {
		*r = (u64)p[1]<<48 | (u64)p[2]<<40 | (u64)p[3]<<32 | (u64)p[4]<<24 | (u64)p[5]<<16 | (u64)p[6]<<8 | (u64)p[7];
		*r += VARINT_SHIFT_L7;
		return p+8;
	}
	*r = ((u64)p[1] << 56) | ((u64)p[2] << 48) | ((u64)p[3] << 40) | ((u64)p[4] << 32) | ((u64)p[5] << 24) | ((u64)p[6] << 16) | ((u64)p[7] << 8) | (u64)p[8];
	*r += VARINT_SHIFT_L8;
	return p+9;
}


#ifdef TEST

#include <string.h>
#include <stdio.h>

int main(int argc, char **argv UNUSED)
{
	byte buf[16] = { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa };
	u64 u;

	if (scanf("%lx", &u) != 1) {
		fprintf(stderr, "Invalid usage!\n");
		return 1;
	}
	varint_put(buf, u);
	int l = varint_len(buf[0]);
	varint_get(buf, &u);
	printf("%u %d %jx", varint_space(u), l, (uintmax_t) u);
	if (argc > 1) {
		for (int i=0; i<l; i++)
			printf(" %x", buf[i]);
	}
	printf("\n");

	return 0;
}

#endif
