/*
 *	Sherlock Gatherer: Data structures used in indices
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

#define CLAMP(x,min,max) ({ int _t=x; (_t < min) ? min : (_t > max) ? max : _t; })

/* Words */

#define MAX_WORD_LEN		64

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
  WT_ALT				/* Alternate texts for graphical elements */
};

#define WORD_TYPE_NAMES	       			\
	T(WORD, ~0)				\
	T(TEXT, 1 << WT_TEXT)			\
	T(EMPH, 1 << WT_EMPH)			\
	T(SMALL, 1 << WT_SMALL)			\
	T(TITLE, 1 << WT_TITLE)			\
	T(HDR, (1 << WT_SMALL_HEADING) | (1 << WT_BIG_HEADING))  \
	T(HDR1, 1 << WT_SMALL_HEADING)		\
	T(HDR2, 1 << WT_BIG_HEADING)		\
	T(KEYWD, 1 << WT_KEYWORD)		\
	T(META, 1 << WT_META)			\
	T(ALT, 1 << WT_ALT)

/* String types */

enum string_type {
  ST_RESERVED,				/* Reserved string type */
  ST_URL,				/* URL of the document */
  ST_HOST,				/* Host name */
  ST_DOMAIN,				/* Domain name */
  ST_REF,				/* URL reference */
  ST_BACKREF,				/* Back-reference (frame or redirect source) */
};

#define STRING_TYPE_NAMES			\
	T(URL, 1 << ST_URL)			\
	T(HOST, 1 << ST_HOST)			\
	T(DOMAIN, 1 << ST_DOMAIN)		\
	T(REF, 1 << ST_REF)			\
	T(BACKREF, 1 << ST_BACKREF)

#define STRING_TYPES_URL ((1 << ST_URL) | (1 << ST_REF) | (1 << ST_BACKREF))
#define STRING_TYPES_CASE_INSENSITIVE ((1 << ST_HOST) | (1 << ST_DOMAIN))

/* Index card attributes */

struct card_attr {
  u32 card;				/* Reference to card description (either oid or filepos) */
  u32 site_id;
  byte weight;
  byte flags;
  byte rfu[2];
};

enum card_flag {
  CARD_FLAG_EMPTY = 1,			/* Empty document (redirect, robot file etc.) [scanner] */
  CARD_FLAG_ACCENTED = 2,		/* Document contains accented characters [scanner] */
  CARD_FLAG_DUP = 4,			/* Removed as a duplicate [merger] */
  CARD_FLAG_MERGED = 8,			/* Destination of a merge [merger] */
};

#define CARD_POS_SHIFT 5		/* Card positions are shifted this # of bytes to the right */

/* String fingerprints */

struct fingerprint {
  byte hash[12];
};

void fingerprint(byte *string, struct fingerprint *fp);

static inline u32
fp_hash(struct fingerprint *fp)
{
  return (fp->hash[0] << 24) | (fp->hash[1] << 16) | (fp->hash[2] << 8) | fp->hash[3];
}

/* Reading of tagged text (Unicode values, tags mapped to 0x80000000 and higher) */

#define GET_TAGGED_CHAR(p,u) do {				\
  u = *p;							\
  if (u >= 0xc0)						\
    GET_UTF8(p,u);						\
  else if (u >= 0x80)						\
    {								\
      p++;							\
      if (u >= 0xb0)						\
	u += 0x80020000;					\
      else if (u >= 0xa0)					\
	u = 0x80010000 + ((u & 0x0f) << 6) + (*p++ & 0x3f);	\
      else							\
	u += 0x80000000;					\
    }								\
  else								\
    p++;							\
} while (0)
