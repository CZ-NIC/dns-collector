/*
 *	Fast Wildcard Pattern Matcher (only `?' and `*' supported)
 *
 *	(c) 1999 Martin Mares <mj@ucw.cz>
 */

struct wildpatt;
struct mempool;

struct wildpatt *wp_compile(byte *, struct mempool *);
int wp_match(struct wildpatt *, byte *);
int wp_min_size(byte *);
