/*
 *	Sherlock Library -- Regular Expressions Test
 *
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"

#include <stdio.h>

#define	PREPARE(patt, icase)	r = rx_compile(patt, icase); printf("\npattern: %s\n", patt)
#define	TEST(txt, should)	printf(#txt ": %d (should %d)\n", rx_match(r, #txt), should)
int
main(void)
{
	regex *r;

	PREPARE("a.*b.*c", 0);
	TEST(abc, 1);
	TEST(ajkhkbbbbbc, 1);
	TEST(Aabc, 0);
	rx_free(r);

	PREPARE("a.*b.*c", 1);
	TEST(aBc, 1);
	TEST(ajkhkbBBBBC, 1);
	TEST(Aabc, 1);
	rx_free(r);

	PREPARE("(ahoj|nebo)", 1);
	TEST("Ahoj", 1);
	TEST("nEBo", 1);
	TEST("ahoja", 0);
	TEST("(ahoj|nebo)", 0);
	rx_free(r);

	return 0;
}
