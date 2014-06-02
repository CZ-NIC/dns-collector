/*
 *	UCW Library -- Universal Sorter: Radix-Split Module
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <string.h>

static void P(radix_split)(struct sort_context *ctx UNUSED, struct sort_bucket *bin, struct sort_bucket **bouts, uint bitpos, uint numbits)
{
  uint nbucks = 1 << numbits;
  uint mask = nbucks - 1;
  struct fastbuf *in = sbuck_read(bin);
  P(key) k;

  struct fastbuf *outs[nbucks];
  bzero(outs, sizeof(outs));

  while (P(read_key)(in, &k))
    {
      P(hash_t) h = P(hash)(&k);
      uint i = (h >> bitpos) & mask;
      if (unlikely(!outs[i]))
	outs[i] = sbuck_write(bouts[i]);
      P(copy_data)(&k, in, outs[i]);
    }
}
