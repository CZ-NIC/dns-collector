/*
 *	Sherlock Library -- Regular Expressions Test
 *
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"

#include <stdio.h>

#define	TEST(txt, should)	printf(#txt ": %d (should %d)\n", rx_match(r, #txt), should)
int
main(void)
{
	regex *r;
	r = rx_compile("a.*b.*c", 0);
	TEST(abc, 1);
	TEST(ajkhkbbbbbc, 1);
	TEST(Aabc, 0);
	rx_free(r);
	r = rx_compile("a.*b.*c", 1);
	TEST(aBc, 1);
	TEST(ajkhkbBBBBC, 1);
	TEST(Aabc, 1);
	rx_free(r);
	return 0;
}
