/*
 *	Checking the correctness of str_len() and str_hash() and proving, that
 *	it is faster than the classical version ;-)
 */

#include "lib/str_hash.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* It will be divided by (10 + strlen()).  */
#define TEST_TIME	1000000

/* The shift of the string according to the alignment.  */
static uns alignment = 0;

static void
random_string(char *str, int len)
{
	int i;
	for (i=0; i<len; i++)
		str[i] = random() % 255 + 1;
	str[len] = 0;
}

static uns
elapsed_time(void)
{
	static struct timeval last_tv, tv;
	uns elapsed;
	gettimeofday(&tv, NULL);
	elapsed = (tv.tv_sec - last_tv.tv_sec) * 1000000 + (tv.tv_usec - last_tv.tv_usec);
	last_tv = tv;
	return elapsed;
}

int
main(int argc, char **argv)
{
	char *strings[] = {
		"",
		"a",
		"aa",
		"aaa",
		"aaaa",
		"aaaaa",
		"aaaaaa",
		"aaaaaaa",
		"aaaaaaaa",
		"aaaaaaaaa",
		"aaaaaaaaaa",
		"\200aaaa",
		"\200",
		"\200\200",
		"\200\200\200",
		"\200\200\200\200",
		"\200\200\200\200\200",
		"kelapS treboR",
		"Robert Spalek",
		"uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu",
		"********************************",
		"****************************************************************",
		NULL
	};
	int lengths[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		30, 40, 50, 60, 70, 80, 90, 100,
		200, 300, 400, 500, 600, 700, 800, 900, 1000,
		2000, 4000, 8000, 16000, 32000, 64000,
		-1
	};
	int i;
	if (argc > 1)
		alignment = atoi(argv[1]);
	printf("Alignment set to %d\n", alignment);
	for (i=0; strings[i]; i++)
		if (strlen(strings[i]) != str_len(strings[i]))
			die("Internal error on string %d", i);
	printf("%d strings tested OK\n", i);
	for (i=0; strings[i]; i++)
		printf("hash %2d = %08x\n", i, str_hash(strings[i]));
	for (i=0; lengths[i] >= 0; i++)
	{
		char str[lengths[i] + 1 + alignment];
		uns count = TEST_TIME / (lengths[i] + 10);
		uns el1 = 0, el2 = 0, elh = 0;
		uns tot1 = 0, tot2 = 0, hash = 0;
		uns j;
		for (j=0; j<count; j++)
		{
			random_string(str + alignment, lengths[i]);
			elapsed_time();
			/* Avoid "optimizing" by gcc, since the functions are
			 * attributed as ((const)).  */
			tot1 += strlen(str + alignment);
			el1 += elapsed_time();
			tot2 += str_len(str + alignment);
			el2 += elapsed_time();
			hash ^= str_hash(str + alignment);
			elh += elapsed_time();
		}
		if (tot1 != tot2)
			die("Internal error during test %d", i);
		printf("Test %d: strlen = %d, passes = %d, classical = %d usec, speedup = %.4f\n",
			i, lengths[i], count, el1, (el1 + 0.) / el2);
		printf("\t\t total hash = %08x, hash time = %d usec\n", hash, elh);
	}
/*
	printf("test1: %d\n", hash_modify(10000000, 10000000, 99777555));
	printf("test1: %d, %d\n", i, hash_modify(i, lengths[i-2], 99777333));
	printf("test1: %d, %d\n", i, hash_modify(lengths[i-2], i, 99777333));
	printf("test1: %d,%d,%d->%d\n", i, i*3-2, i*i, hash_modify(4587, i*3-2, i*i));
	printf("test1: %d\n", hash_modify(lengths[5], 345, i));
*/
	return 0;
}
