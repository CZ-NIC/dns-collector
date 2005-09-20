/*
 *	Knuth-Morris-Pratt's search automat for N given strings
 * 
 *	(c) 1999--2005, Robert Spalek <robert@ucw.cz>
 *
 *	Set KMP_TRANSLATE to one of {UNICODE,ASCII,NO}_TRANSLATE.
 *
 *	Don't touch this file, because it is a mess.
 */

#ifndef _UCW_KMP_H
#define _UCW_KMP_H

#include "lib/lists.h"

#define	MF_TOLOWER	1
#define	MF_UNACCENT	2
#define	MF_ONLYALPHA	4
	/* how to modify characters in the string */
#define	CONTROL_CHAR	':'
	/* all non-alphabetic characters are treated as CONTROL_CHAR */

#define UNICODE_TRANSLATE(new_c, c, flags)	do { \
	if (!c) \
		new_c = 0; \
	else { \
		if (flags & MF_TOLOWER) \
			c = Utolower(c); \
		if (flags & MF_UNACCENT) \
			c = Uunaccent(c); \
		if (flags & MF_ONLYALPHA && !Ualpha(c)) \
			c = CONTROL_CHAR; \
		new_c = c; \
	} \
} while (0)
#define ASCII_TRANSLATE(new_c, c, flags)	do { \
	if (!c) \
		new_c = 0; \
	else { \
		if (flags & MF_TOLOWER) \
			c = Clocase(c); \
		if (flags & MF_ONLYALPHA && !Calpha(c)) \
			c = CONTROL_CHAR; \
		new_c = c; \
	} \
} while (0)
#define NO_TRANSLATE(new_c, c, flags)	do { new_c = c; } while (0)

typedef uns kmp_state_t;
typedef word kmp_char_t;

struct kmp_transition {
	struct node n;			/* link list of sons for a given node */
	struct kmp_transition *next;	/* collision in the hash-table of all transitions */
	kmp_state_t from, to;
	kmp_char_t c;
};
struct kmp_transitions {
	int count, size;
	struct list *sons;		/* link-list of all sons for each given node */
	uns hash_size;
	struct kmp_transition **chain;	/* hash-table of [node, char]->son */
};

struct kmp_output {
	struct kmp_output *next;	/* output link list for every node */
	uns id;
	uns len;
};

struct mempool;
struct kmp {
	struct mempool *mp;
	int modify_flags;		/* which nocase/noaccent mode is this kmp for */
	int words_len;			/* total length of searched words */
	struct kmp_transitions g;	/* hash table of forward transitions of automat */
	kmp_state_t *f;			/* back transitions of automat */
	struct kmp_output **out;	/* found words for every state */
};

/* kmp.c */
struct kmp *kmp_new(struct mempool *mp, int words_len, uns modify_flags);
void kmp_enter_raw_string(struct kmp *kmp, const byte *str, uns id);
void kmp_build(struct kmp *kmp);

static inline void
kmp_get_char(const byte **str, kmp_char_t *c, uns modify_flags UNUSED)
{
	while (1)
	{
		uns w;
		kmp_char_t new_c;
		GET_TAGGED_CHAR((*str), w);
		if (w >= 0x80000000)
			w = CONTROL_CHAR;
		KMP_TRANSLATE(new_c, w, modify_flags);
		if (new_c != CONTROL_CHAR || *c != CONTROL_CHAR)
		{
			*c = new_c;
			return;
		}
	}
}

static inline void
kmp_enter_string(struct kmp *kmp, const byte *str, uns id)
{
	/* Ugly hack to avoid linking libucw with libcharset:
	 * Convert the entered string here in the header and then call the
	 * library function.  This function calls kmp_get_char() without any
	 * conversion.  */
	byte buf[strlen(str)+1], *str2 = buf;
	kmp_char_t c;
	do
	{
		kmp_get_char(&str, &c, kmp->modify_flags);
		str2 = utf8_put(str2, c);
	}
	while (c);
	kmp_enter_raw_string(kmp, str2, id);
}

struct kmp_result {
	struct node n;			/* strings with non-zero frequency are put into a link-list */
	uns occur;
};

static inline uns
transition_hashf(struct kmp_transitions *l UNUSED, struct kmp_transition *tr)
{
	return tr->from + (tr->c << 16);
}

static inline int
transition_compare(struct kmp_transition *a, struct kmp_transition *b)
{
	if (a->from == b->from && a->c == b->c)
		return 0;
	else
		return 1;
}

static inline struct kmp_transition **
transition_search(struct kmp_transitions *l, struct kmp_transition *tr)
{
	uns hf = transition_hashf(l, tr) % l->hash_size;
	struct kmp_transition **last = l->chain + hf;
	while (*last && transition_compare(*last, tr))
		last = &(*last)->next;
	ASSERT(last);
	return last;
}

static inline void
add_result(struct list *nonzeroes, struct kmp_result *freq, struct kmp_output *out)
{
	for (; out; out = out->next)
		if (!freq[out->id].occur++)
			add_tail(nonzeroes, &freq[out->id].n);
}

static inline byte *
kmp_search_internal(struct kmp *kmp, byte *str, uns len, struct list *nonzeroes, struct kmp_result *freq, struct kmp_output *out)
  /* For every found string with id ID, it increments freq[ID].
   * Also, it finds the longest among the leftmost matches.  */
{
	byte *str_end = str + len;
	kmp_state_t s = 0;
	kmp_char_t c = CONTROL_CHAR;
	struct kmp_transition tr, **prev;
	byte eof = 0;
	if (kmp->words_len <= 1)
		return NULL;
	//TRACE(20, "kmp.c: Searching string %s", str);
	byte *largest_match = NULL;
	while (1)
	{
		tr.from = s;
		tr.c = c;
		prev = transition_search(&kmp->g, &tr);
		while (tr.from && !*prev)
		{
			tr.from = kmp->f[ tr.from ];
			prev = transition_search(&kmp->g, &tr);
		}
		s = *prev ? (*prev)->to : 0;
		if (nonzeroes)
			add_result(nonzeroes, freq, kmp->out[s]);
		/* Beware that out->len is measured in modified characters of
		 * the search pattern, hence it is not very reliable if you use
		 * unaccenting.  */
		struct kmp_output *kout = kmp->out[s];
		if (kout && (!largest_match || str - kout->len <= largest_match))
		{
			largest_match = str - kout->len;
			if (out)
				*out = *kout;
		}
		if (eof)
			break;
		if (str >= str_end)
			c = 0;
		else
			kmp_get_char((const byte **)&str, &c, kmp->modify_flags);
		if (!c)
		{
			/* Insert CONTROL_CHAR at the beginning and at the end too.  */
			c = CONTROL_CHAR;
			eof = 1;
		}
	}
	return largest_match;
}

static inline void
kmp_search(struct kmp *kmp, const byte *str, struct list *nonzeroes, struct kmp_result *freq)
{
	kmp_search_internal(kmp, (byte*) str, strlen(str), nonzeroes, freq, NULL);
}

static inline byte *
kmp_find_first(struct kmp *kmp, byte *str, uns len, struct kmp_output *out)
{
	return kmp_search_internal(kmp, str, len, NULL, NULL, out);
}

#endif
