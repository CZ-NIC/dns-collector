/*
 *	LiZzaRd -- Fast compression method based on Lempel-Ziv 77
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	The file format is based on LZO1X and 
 *	the compression method is based on zlib.
 */

#define	LIZZARD_NEEDS_CHARS	8
  /* The compression routine needs input buffer 8 characters longer, because it
   * does not check the input bounds all the time.  */
#define	LIZZARD_MAX_MULTIPLY	23./22
#define	LIZZARD_MAX_ADD		4
  /* In the worst case, the compressed file will not be longer than its
   * original length * 23/22 + 4.
   *
   * The additive constant is for EOF and the header of the file.
   *
   * The multiplicative constant comes from 19-byte incompressible string
   * followed by a 3-sequence that can be compressed into 2-byte link.  This
   * breaks the copy-mode and it needs to be restarted with a new header.  The
   * total length is 2(header) + 2(link) + 19(string) = 23.
   */

int lizzard_compress(byte *in, uns in_len, byte *out);
int lizzard_decompress(byte *in, byte *out);
