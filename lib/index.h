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

/* String types */

enum string_type {
  ST_RESERVED,				/* Reserved string type */
  ST_URL,				/* URL of the document */
  ST_HOST,				/* Host name */
  ST_DOMAIN,				/* Domain name */
  ST_REF				/* URL reference */
};

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

/* String fingerprints */

struct fingerprint {
  byte hash[12];
};

void fingerprint(byte *string, struct fingerprint *fp);
