/*
 *	Sherlock: Data structures used in indices
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
 */

/* Words */

#define MAX_WORD_LEN		64

/* Word and string types are defined in lib/custom.h */

/* Index card attributes */

struct card_attr {
  u32 card;				/* Reference to card description (either oid or filepos) */
  u32 site_id;
#define INT_ATTR(t,i,o,k,g,p) t i;
  CUSTOM_ATTRS				/* Include all custom attributes */
#undef INT_ATTR
  byte weight;
  byte flags;
  // byte rfu[2];			/* If no custom attributes are defined */
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
    GET_UTF8_CHAR(p,u);						\
  else if (u >= 0x80)						\
    {								\
      p++;							\
      if (u >= 0xb0)						\
        {							\
	  if (u != 0xb0)					\
            ASSERT(0);						\
	  u += 0x80020000;					\
        }							\
      else if (u >= 0xa0)					\
        {							\
	  ASSERT(*p >= 0x80 && *p <= 0xbf);			\
	  u = 0x80010000 + ((u & 0x0f) << 6) + (*p++ & 0x3f);	\
        }							\
      else							\
	u += 0x80000000;					\
    }								\
  else								\
    p++;							\
} while (0)
