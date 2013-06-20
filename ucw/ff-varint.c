/*
 *	UCW Library: Reading and writing Varints on Fastbuf Streams
 *
 *	(c) 2013 Tomas Valla <tom@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>
#include <ucw/varint.h>
#include <ucw/ff-varint.h>
#include <ucw/ff-binary.h>


u64 bget_varint_slow(struct fastbuf *b, u64 repl)
{
	uns h = bgetc(b);
	uns l = varint_len(h);
	byte buf[l];
	buf[0] = h;
	l--;
	if (breadb(b, buf+1, l) < l)
		return repl;
	varint_get(buf, &repl);
	return repl;
}

void bput_varint_slow(struct fastbuf *b, u64 u)
{
	byte buf[9];
	uns l = varint_put(buf, u);
	bwrite(b, buf, l);
}

#ifdef TEST

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	#define FUNCS \
		F(BGET_VARINT) F(BPUT_VARINT)

	enum {
		#define F(x) FUNC_##x,
		FUNCS
		#undef F
	};
	char *names[] = {
		#define F(x) [FUNC_##x] = #x,
		FUNCS
		#undef F
	};

	uns func = ~0U;
	if (argc > 1)
		for (uns i = 0; i < ARRAY_SIZE(names); i++)
			if (!strcasecmp(names[i], argv[1]))
				func = i;
	if (!~func) {
		fprintf(stderr, "Invalid usage!\n");
		return 1;
	}

	struct fastbuf *b = fbgrow_create(8);
	switch (func) {
		uns u;
		u64 r;
		int i;
		case FUNC_BGET_VARINT:
			while (scanf("%x", &u) == 1)
				bputc(b, u);
			fbgrow_rewind(b);
			while (bpeekc(b) >= 0) {
				if (btell(b))
					putchar(' ');
				r = bget_varint_slow(b, ~0LLU);
				printf("%llx", r);
			}
			putchar('\n');
			break;

		case FUNC_BPUT_VARINT:
			i = 0;
			while (scanf("%llx", &r) == 1)
				bput_varint_slow(b, r);
			fbgrow_rewind(b);
			while (bpeekc(b) >= 0) {
				if (i++)
					putchar(' ');
				printf("%02x", bgetc(b));
			}
			putchar('\n');
			break;
		default:
			ASSERT(0);
	}

	bclose(b);
	return 0;
}

#endif
