/*
 *	Sherlock: Custom Parts of Configuration
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
 */

/* Word types */

enum word_type {
  WT_RESERVED,				/* Reserved word type */
  WT_TEXT,				/* Ordinary text */
  WT_EMPH,				/* Emphasized text */
  WT_SMALL,				/* Small font */
  WT_TITLE,				/* Document title */
  WT_SMALL_HEADING,			/* Heading */
  WT_BIG_HEADING,			/* Larger heading */
  WT_KEYWORD,				/* Explicitly marked keyword */
  WT_META,				/* Various meta-information */
  WT_ALT,				/* Alternate texts for graphical elements */
  WT_URL1,				/* Word extracted from document URL (low and high weight) */
  WT_URL2,
  WT_FILE,				/* Part of file name */
  WT_LINK,				/* Link text */
  WT_CAT_TITLE,				/* Catalog title */
  WT_CAT_DESC,				/* Catalog description */
  WT_MAX
};

/* Descriptive names used for user output */
#define WORD_TYPE_USER_NAMES							\
   "reserved", "text", "emph", "small", "title", "hdr1", "hdr2", "keywd",	\
   "meta", "alt", "urlword1", "urlword2", "nameword", "link", "ctitle", "cdesc"

/* Keywords for word type names */
#define WORD_TYPE_NAMES	       			\
	T(WORD, ~((1 << WT_FILE) | (1 << WT_LINK)))	\
	T(TEXT, 1 << WT_TEXT)			\
	T(EMPH, 1 << WT_EMPH)			\
	T(SMALL, 1 << WT_SMALL)			\
	T(TITLE, 1 << WT_TITLE)			\
	T(HDR, (1 << WT_SMALL_HEADING) | (1 << WT_BIG_HEADING))  \
	T(HDR1, 1 << WT_SMALL_HEADING)		\
	T(HDR2, 1 << WT_BIG_HEADING)		\
	T(KEYWD, 1 << WT_KEYWORD)		\
	T(META, 1 << WT_META)			\
	T(ALT, 1 << WT_ALT)			\
	T(URLWORD, (1 << WT_URL1) | (1 << WT_URL2))	\
	T(FILE, 1 << WT_FILE)			\
	T(LINK, 1 << WT_LINK)			\
	T(CTITLE, 1 << WT_CAT_TITLE)		\
	T(CDESC, 1 << WT_CAT_DESC)

/* These types are not shown in document contexts */
#define WORD_TYPES_HIDDEN ((1 << WT_URL1) | (1 << WT_URL2) | (1 << WT_FILE))

/* These types are separated out when printing contexts */
#define WORD_TYPES_META ((1 << WT_TITLE) | (1 << WT_KEYWORD) | \
	(1 << WT_META) | (1 << WT_CAT_TITLE) | (1 << WT_CAT_DESC))

/* These types are always matched without accents if accent mode is set to "auto" */
#define WORD_TYPES_NO_AUTO_ACCENT ((1 << WT_URL1) | (1 << WT_URL2) | (1 << WT_FILE)

/* String types */

enum string_type {
  ST_RESERVED,				/* Reserved string type */
  ST_URL,				/* URL of the document */
  ST_HOST,				/* Host name */
  ST_DOMAIN,				/* Domain name */
  ST_REF,				/* URL reference */
  ST_MAX
};

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
 *  type get_func(struct card_attr *ca, byte *attr)
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

struct card_attr;
struct odes;

#ifdef CONFIG_IMAGES

/*
 *  We store several image properties to card_attr->image_flags and
 *  match them as custom attributes. The image_flags byte contains:
 *
 *  bits 0--1	image format (0: not an image, 1: JPEG, 2: PNG, 3: GIF)
 *  bits 2--3   image size (0: <=100x100, 1: <=320x200, 2: <=640x480, 3: other)
 *  bits 4--5   image colors (0: grayscale, 1: <=16, 2: <=256, 3: >256)
 */

#define CUSTOM_CARD_ATTRS \
	byte image_flags;

#define CUSTOM_ATTRS \
	SMALL_SET_ATTR(ifmt, IMGTYPE, custom_it_get, custom_it_parse)		\
	SMALL_SET_ATTR(isize, IMGSIZE, custom_is_get, custom_is_parse)		\
	SMALL_SET_ATTR(icolors, IMGCOLORS, custom_ic_get, custom_ic_parse)

void custom_create_attrs(struct odes *odes, struct card_attr *ca);

/* These must be macros instead of inline functions, struct card_attr is not fully defined yet */
#define custom_it_get(ca) ((ca)->image_flags & 3)
#define custom_is_get(ca) (((ca)->image_flags >> 2) & 3)
#define custom_ic_get(ca) (((ca)->image_flags >> 4) & 3)

byte *custom_it_parse(u32 *dest, byte *value, uns intval);
byte *custom_is_parse(u32 *dest, byte *value, uns intval);
byte *custom_ic_parse(u32 *dest, byte *value, uns intval);

#else

/* No custom attributes defined */

#define CUSTOM_CARD_ATTRS
#define CUSTOM_ATTRS
static inline void custom_create_attrs(struct odes *odes UNUSED, struct card_attr *ca UNUSED) { }

#endif
