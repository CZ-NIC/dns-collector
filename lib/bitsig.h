/*
 *	Bit Array Signatures -- A Dubious Detector of Duplicates
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

struct bitsig;

struct bitsig *bitsig_init(uns perrlog, uns maxn);
int bitsig_member(struct bitsig *b, byte *item);
int bitsig_insert(struct bitsig *b, byte *item);
