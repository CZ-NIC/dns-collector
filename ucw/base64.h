/*
 *	UCW Library -- Base 64 Encoding & Decoding
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifdef CONFIG_UCW_CLEAN_ABI
#define base64_decode ucw_base64_decode
#define base64_encode ucw_base64_encode
#endif

/**
 * Encodes @len bytes of data pointed to by @src by base64 encoding.
 * Stores them in @dest and returns the number of bytes the output
 * takes.
 */
uint base64_encode(byte *dest, const byte *src, uint len);
/**
 * Decodes @len bytes of data pointed to by @src from base64 encoding.
 * All invalid characters are ignored. The result is stored into @dest
 * and length of the result is returned.
 */
uint base64_decode(byte *dest, const byte *src, uint len);

/**
 * Use this macro to calculate @base64_encode() output buffer size.
 */
#define BASE64_ENC_LENGTH(x) (((x)+2)/3 *4)

/*
 * When called for BASE64_IN_CHUNK-byte chunks, the result will be
 * always BASE64_OUT_CHUNK bytes long. If a longer block is split
 * to such chunks, the result will be identical.
 */
#define BASE64_IN_CHUNK 3 /** Size of chunk on the un-encoded side. **/
#define BASE64_OUT_CHUNK 4 /** Size of chunk on the encoded side. **/

