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
#define	LIZZARD_MAX_MULTIPLY	65LL/64
#define	LIZZARD_MAX_ADD		4
  /* In the worst case, the compressed file will not be longer than its
   * original length * 65/64 + 4.
   *
   * The additive constant is for EOF and the header of the file.
   *
   * The multiplicative constant 129/128 comes from an incompressible string of
   * length 256 that requires a 2-byte header.  However, if longer strings get
   * interrupted by a sequence of length 3 compressed into 2 characters, the
   * overlap is sligtly bigger.
   * TODO: check */

int lizzard_compress(byte *in, uns in_len, byte *out);
int lizzard_decompress(byte *in, byte *out);
