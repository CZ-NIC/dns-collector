/*
 *	Sherlock Library -- String Fingerprints
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
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
 *  be cause by a collision is mixing up two strings or labels
 *  of two documents which is relatively harmless.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/index.h"
#include "lib/md5.h"

#include <string.h>

static uns finger_www_hack;

static struct cfitem finger_config[] = {
  { "Fingerprints",	CT_SECTION,	NULL },
  { "WWWHack",		CT_INT,		&finger_www_hack },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR finger_conf_init(void)
{
  cf_register(finger_config);
}

void
fingerprint(byte *string, struct fingerprint *fp)
{
  struct MD5Context c;
  uns len = strlen(string);
  byte digest[16];

  MD5Init(&c);
  if (finger_www_hack && len >= 11 && !memcmp(string, "http://www.", 11))
    {
      /* FIXME: This is a dirty hack, but it has to stay until we get real handling of duplicates */
      MD5Update(&c, string, 7);
      MD5Update(&c, string+11, len-11);
    }
  else
    MD5Update(&c, string, len);
  MD5Final(digest, &c);
  memcpy(fp->hash, digest, 12);
}
