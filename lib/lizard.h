/*
 *	LiZzaRd -- Fast compression method based on Lempel-Ziv 77
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	The file format is based on LZO1X and 
 *	the compression method is based on zlib.
 */

#define	LIZZARD_MAX_PROLONG_FACTOR	129/128
  /* Incompressible string of length 256 has a 2-byte header.
   * Hence the scheme the file length get multiplied by 129/128 in the worst
   * case + at most 4 bytes are added for header and EOF.  */

int lizzard_compress(byte *in, uns in_len, byte *out);
int lizzard_decompress(byte *in, byte *out);
