/*
 *	Sherlock Library -- String Fingerprints
 *
 *	(c) 2001--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *  We use a hashing function to map all the URL's and other
 *  hairy strings we work with to a much simpler universe
 *  of constant length bit strings (currently 96-bit ones).
 *  With a random hashing function (which is equivalent to
 *  having a fixed function and random input), the probability
 *  of at least one collision happening is at most c*n^2/m
 *  where n is the number of strings we hash, m is the size
 *  of our bit string universe (2^96) and c is a small constant.
 *  We set m sufficiently large and expect no collisions
 *  to occur. On the other hand, the worst thing which could
 *  be caused by a collision is mixing up two strings or labels
 *  of two documents which is relatively harmless.
 */

#include "lib/lib.h"
#include "lib/index.h"
#include "lib/md5.h"

void
fingerprint(byte *string, struct fingerprint *fp)
{
  struct MD5Context c;
  byte digest[16];

  MD5Init(&c);
  MD5Update(&c, string, strlen(string));
  MD5Final(digest, &c);
  memcpy(fp->hash, digest, 12);
}
