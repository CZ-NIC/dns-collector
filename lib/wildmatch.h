/*
 *	UCW Library -- Fast Wildcard Pattern Matcher (only `?' and `*' supported)
 *
 *	(c) 1999 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

struct wildpatt;
struct mempool;

struct wildpatt *wp_compile(const byte *, struct mempool *);
int wp_match(struct wildpatt *, const byte *);
int wp_min_size(const byte *);
