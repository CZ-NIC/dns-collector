/*
 *	LiZaRd -- Fast compression method based on Lempel-Ziv 77
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_LIZARD_H
#define _SHERLOCK_LIZARD_H

#define	LIZARD_NEEDS_CHARS	8
  /* The compression routine needs input buffer 8 characters longer, because it
   * does not check the input bounds all the time.  */
#define	LIZARD_MAX_MULTIPLY	23./22
#define	LIZARD_MAX_ADD		4
  /* In the worst case, the compressed file will not be longer than its
   * original length * 23/22 + 4.
   *
   * The additive constant is for EOF and the header of the file.
   *
   * The multiplicative constant comes from 19-byte incompressible string
   * followed by a 3-sequence that can be compressed into 2-byte link.  This
   * breaks the copy-mode and it needs to be restarted with a new header.  The
   * total length is 2(header) + 19(string) + 2(link) = 23.
   */

/* lizard.c */
int lizard_compress(byte *in, uns in_len, byte *out);
int lizard_decompress(byte *in, byte *out);

/* lizard-safe.c */
struct lizard_buffer;

struct lizard_buffer *lizard_alloc(void);
void lizard_free(struct lizard_buffer *buf);
byte *lizard_decompress_safe(byte *in, struct lizard_buffer *buf, uns expected_length);

/* adler32.c */
uns update_adler32(uns adler, byte *ptr, uns len);

static inline uns
adler32(byte *buf, uns len)
{
  return update_adler32(1, buf, len);
}

/* lizard-fb.c */
struct fastbuf;

void lizard_set_type(uns type, float min_compr);
int lizard_bwrite(struct fastbuf *fb_out, byte *ptr_in, uns len_in);
int lizard_bbcopy_compress(struct fastbuf *fb_out, struct fastbuf *fb_in, uns len_in);
int lizard_memread(struct lizard_buffer *liz_buf, byte *ptr_in, byte **ptr_out, uns *type);
int lizard_bread(struct lizard_buffer *liz_buf, struct fastbuf *fb_in, byte **ptr_out, uns *type);
  /* These functions use static variables, hence they are not re-entrant.  */

#endif
