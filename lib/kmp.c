/*
 *	Knuth-Morris-Pratt's Substring Search for N given strings
 *
 *	(c) 1999--2005, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/bitops.h"
#include "lib/mempool.h"
#include "lib/lists.h"
#include "sherlock/tagged-text.h"
#include "lib/unicode.h"

#define KMP_GET_CHAR KMP_GET_RAW
#include "lib/kmp.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>

#define	TRACE(level, mask...)	if (0) fprintf(stderr, mask)

struct kmp *
kmp_new(struct mempool *mp, int words_len, uns modify_flags)
{
	struct kmp *kmp = mp_alloc_zero(mp, sizeof(struct kmp));
	kmp->mp = mp;
	kmp->modify_flags = modify_flags;
	kmp->words_len = words_len;
	int size = words_len;
	kmp->g.count = 1;
	kmp->g.size = size;
	kmp->g.sons = mp_alloc_zero(mp, size * sizeof(struct list));
	init_list(kmp->g.sons + 0);
	if (words_len > 1)
		size = words_len * bit_fls(words_len);
	else
		size = 1;
	kmp->g.hash_size = size;
	kmp->g.chain = mp_alloc_zero(mp, size * sizeof(struct kmp_transition *));
	kmp->f = mp_alloc_zero(mp, words_len * sizeof(kmp_state_t));
	kmp->out = mp_alloc_zero(mp, words_len * sizeof(struct kmp_output *));
	return kmp;
}

/*
 * The only merge operation is that son includes output of his father (and also
 * his father,...), so we can merge the link-lists.
 */
static void
merge_output(struct kmp_output **target, struct kmp_output *src)
{
	while (*target)
		target = &(*target)->next;
	*target = src;
}

static struct kmp_output *
new_output(struct kmp *kmp, uns id, uns len)
{
	struct kmp_output *out = mp_alloc(kmp->mp, sizeof(struct kmp_output));
	out->next = NULL;
	out->id = id;
	out->len = len;
	return out;
}
 
void
kmp_enter_raw_string(struct kmp *kmp, const byte *str, uns id)
{
	struct kmp_transition tr = { .next=NULL, .from=0 }, **prev;
	struct kmp_output *new_out;
	const byte *orig_str = str;
	uns len = 0;
	kmp_char_t c = 'a';

	TRACE(20, "kmp.c: Entering string %s", str);
	kmp_get_char(&str, &c, 0);
	len++;
	if (!c)
		return;
	while (c)
	{
		tr.c = c;
		prev = transition_search(&kmp->g, &tr);
		if (!*prev)
			break;
		tr.from = (*prev)->to;
		kmp_get_char(&str, &c, 0);
		len++;
	}
	while (c)
	{
		*prev = mp_alloc_zero(kmp->mp, sizeof(struct kmp_transition));
		tr.to = kmp->g.count++;
		**prev = tr;
		add_tail(kmp->g.sons + tr.from, &(*prev)->n);
		init_list(kmp->g.sons + tr.to);
		kmp_get_char(&str, &c, 0);
		len++;
		tr.from = tr.to;
		tr.c = c;
		prev = transition_search(&kmp->g, &tr);
		ASSERT(!*prev);
	}
	if (kmp->out[tr.from])
		TRACE(5, "kmp.c: string %s is inserted more than once", orig_str);
	new_out = new_output(kmp, id, len-1);
	merge_output(kmp->out + tr.from, new_out);
}

static void
construct_f_out(struct kmp *kmp)
{
	kmp_state_t *fifo;
	int read, write;
	struct kmp_transition *son;

	fifo = alloca(kmp->words_len * sizeof(kmp_state_t));
	read = write = 0;
	kmp->f[0] = 0;
	WALK_LIST(son, kmp->g.sons[0])
	{
		ASSERT(son->from == 0);
		kmp->f[son->to] = 0;
		fifo[write++] = son->to;
	}
	while (read != write)
	{
		kmp_state_t r, s, t;
		r = fifo[read++];
		WALK_LIST(son, kmp->g.sons[r])
		{
			struct kmp_transition tr, **prev;
			ASSERT(son->from == r);
			tr.c = son->c;
			s = son->to;
			fifo[write++] = s;
			t = kmp->f[r];
			while (1)
			{
				tr.from = t;
				prev = transition_search(&kmp->g, &tr);
				if (*prev || !tr.from)
					break;
				t = kmp->f[t];
			}
			kmp->f[s] = *prev ? (*prev)->to : 0;
			merge_output(kmp->out + s, kmp->out[ kmp->f[s] ]);
		}
	}
}

void
kmp_build(struct kmp *kmp)
{
	ASSERT(kmp->g.count <= kmp->words_len);
	construct_f_out(kmp);
	if (kmp->words_len > 1)
		TRACE(0, "Built KMP with modify flags %d for total words len %d, it has %d nodes", kmp->modify_flags, kmp->words_len, kmp->g.count);
}
