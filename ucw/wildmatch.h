/*
 *	UCW Library -- Fast Wildcard Pattern Matcher (only `?' and `*' supported)
 *
 *	(c) 1999 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifdef CONFIG_UCW_CLEAN_ABI
#define wp_compile ucw_wp_compile
#define wp_match ucw_wp_match
#define wp_min_size ucw_wp_min_size
#endif

struct wildpatt;
struct mempool;

struct wildpatt *wp_compile(const char *, struct mempool *);
int wp_match(struct wildpatt *, const char *);
int wp_min_size(const char *);
