/*
 *	UCW Library -- Base 224 Encoding & Decoding
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

uns base224_encode(byte *dest, byte *src, uns len);
uns base224_decode(byte *dest, byte *src, uns len);

/*
 * Warning: when encoding, at least 4 bytes of extra space are needed.
 * Better use this macro to calculate buffer size.
 */
#define BASE224_ENC_LENGTH(x) (((x)*8+38)/39*5)

/*
 * When called for BASE224_IN_CHUNK-byte chunks, the result will be
 * always BASE224_OUT_CHUNK bytes long. If a longer block is split
 * to such chunks, the result will be identical.
 */
#define BASE224_IN_CHUNK 39
#define BASE224_OUT_CHUNK 40
