/*
 *	Sherlock: Custom Parts of Configuration
 *
 *	(c) 2001--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/* Structures we'll need in function parameters */

struct card_attr;
struct odes;

/* Word types (at most 7 of them + WT_RESERVED and WT_MAX) */

#define WT_RESERVED		0	/* Reserved word type */
#define WT_TEXT			1	/* Ordinary text */
#define WT_EMPH			2	/* Emphasized text */
#define WT_SMALL		3	/* Small font */
#define WT_SMALL_HEADING	4	/* Heading */
#define WT_BIG_HEADING		5	/* Larger heading */
#define WT_ALT			6	/* Alternate texts for graphical elements */
#define WT_LINK			7	/* Link text */
#define WT_MAX			8

/* Descriptive names used for user output */
#define WORD_TYPE_USER_NAMES							\
   "reserved", "text", "emph", "small", "hdr1", "hdr2", "alt", "link"

/* Keywords for word type names */
#define WORD_TYPE_NAMES	       			\
	T(WORD, ~(1 << WT_LINK))		\
	T(TEXT, 1 << WT_TEXT)			\
	T(EMPH, 1 << WT_EMPH)			\
	T(SMALL, 1 << WT_SMALL)			\
	T(HDR, (1 << WT_SMALL_HEADING) | (1 << WT_BIG_HEADING))  \
	T(HDR1, 1 << WT_SMALL_HEADING)		\
	T(HDR2, 1 << WT_BIG_HEADING)		\
	T(ALT, 1 << WT_ALT)			\
	T(LINK, 1 << WT_LINK)

/* These types are always matched without accents if accent mode is set to "auto" */
#define WORD_TYPES_NO_AUTO_ACCENT 0

/* These types belong to all languages */
#define WORD_TYPES_ALL_LANGS (1 << WT_LINK)

/* Meta information types (at most 16 of them + MT_MAX) */

#define MT_TITLE		0	/* Document title */
#define MT_KEYWORD		1	/* Keyword from the document */
#define MT_MISC			2	/* Unclassified metas */
#define MT_MAX			3

#define META_TYPE_USER_NAMES			\
   "title", "keywd", "misc"

/* Keywords for meta type names */
#define META_TYPE_NAMES	       			\
	T(TITLE, 1 << MT_TITLE)			\
	T(KEYWD, 1 << MT_KEYWORD)		\
	T(META, 1 << MT_MISC)

#define META_TYPES_NO_AUTO_ACCENT 0
#define META_TYPES_ALL_LANGS 0

/* String types */

#define ST_RESERVED		0	/* Reserved string type */
#define ST_URL			1	/* URL of the document */
#define ST_HOST			2	/* Host name */
#define ST_DOMAIN		3	/* Domain name */
#define ST_REF			4	/* URL reference */
#define ST_MAX			5

#define STRING_TYPE_USER_NAMES							\
   "URL", "host", "domain", "ref", "type4", "type5", "type6", "type7",	\
   "type8", "type9", "type10", "type11", "type12", "type13", "type14", "type15"

#define STRING_TYPE_NAMES			\
	T(URL, 1 << ST_URL)			\
	T(HOST, 1 << ST_HOST)			\
	T(DOMAIN, 1 << ST_DOMAIN)		\
	T(REF, 1 << ST_REF)

#define STRING_TYPES_URL ((1 << ST_URL) | (1 << ST_REF))
/* These must be indexed in lowercase form */
#define STRING_TYPES_CASE_INSENSITIVE ((1 << ST_HOST) | (1 << ST_DOMAIN))

static inline void
custom_index_strings(struct odes *o UNUSED, void (*f)(byte *text, uns type) UNUSED)
{
  /*
   * Call f for all strings you want to add to the index.
   * ST_URL, ST_HOST, ST_DOMAIN and ST_REF are indexed automatically
   * if they are defined.
   */
}

/*
 *  Definitions of custom attributes:
 *
 *  First of all, you need to define your own card_attr fields which will
 *  contain your attributes: CUSTOM_CARD_ATTRS lists them.
 *  Please order the attributes by decreasing size to get optimum padding.
 *
 *  Then define custom_create_attrs() which will get the object description
 *  and set your card_attr fields accordingly.
 *
 *  Finally, you have to define CUSTOM_ATTRS with matching rules:
 *
 *  INT_ATTR(id, keyword, get_func, parse_func) -- unsigned integer attribute
 *
 *  id		C identifier of the attribute
 *  keywd	search server keyword for the attribute
 *  int get_func(struct card_attr *ca)
 *		get attribute value from the card_attr
 *  byte *parse_func(u32 *dest, byte *value, uns intval)
 *		parse value in query (returns error message or NULL)
 *		for KEYWD = "string", it gets value="string", intval=0
 *		for KEYWD = num, it gets value=NULL, intval=num.
 *
 *  SMALL_SET_ATTR(id, keyword, get_func, parse_func)
 *    -- integers 0..31 with set matching
 *
 *  A good place for definitions of the functions is lib/custom.c.
 */

/* No custom attributes defined yet */

#define CUSTOM_CARD_ATTRS
#define CUSTOM_ATTRS
static inline void custom_create_attrs(struct odes *odes UNUSED, struct card_attr *ca UNUSED) { }
