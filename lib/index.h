/*
 *	Sherlock: Data structures used in indices
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_INDEX_H
#define _SHERLOCK_INDEX_H

#include "lib/fastbuf.h"
#include SHERLOCK_CUSTOM
#include "charset/unistream.h"

/* Words */

#define MAX_WORD_LEN		64
#define MAX_COMPLEX_LEN		10

/* Word and string types are defined in lib/custom.h */

/* Global index parameters */

struct index_params {
  sh_time_t ref_time;			/* Reference time (for document ages etc.) */
};

/* Index card attributes */

struct card_attr {
  u32 card;				/* Reference to card description (either oid or filepos) */
  u32 site_id;
  CUSTOM_CARD_ATTRS			/* Include all custom attributes */
  byte weight;
  byte flags;
  byte age;				/* Document age in pseudo-logarithmic units wrt. reference time */
  // byte rfu[1];			/* If no custom attributes are defined */
};

enum card_flag {
  CARD_FLAG_EMPTY = 1,			/* Empty document (redirect, robot file etc.) [scanner] */
  CARD_FLAG_ACCENTED = 2,		/* Document contains accented characters [scanner] */
  CARD_FLAG_DUP = 4,			/* Removed as a duplicate [merger] */
  CARD_FLAG_MERGED = 8,			/* Destination of a merge [merger] */
  CARD_FLAG_IMAGE = 16,			/* Is an image object [scanner] */
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
	  ASSERT(u == 0xb0);					\
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

static inline uns
bget_tagged_char(struct fastbuf *f)
{
  uns u = bgetc(f);
  if ((int)u < 0x80)
    ;
  else if (u < 0xc0)
    {
      if (u >= 0xb0)
	{
	  ASSERT(u == 0xb0);
	  u += 0x80020000;
	}
      else if (u >= 0xa0)
	{
	  uns v = bgetc(f);
	  ASSERT(v >= 0x80 && v <= 0xbf);
	  u = 0x80010000 + ((u & 0x0f) << 6) + (v & 0x3f);
	}
      else
	u += 0x80000000;
    }
  else
    {
      bungetc(f);
      u = bget_utf8(f);
    }
  return u;
}

/* Conversion of document age from seconds to our internal units */

static inline int
convert_age(sh_time_t lastmod, sh_time_t reftime)
{
  sh_time_t age;
  if (reftime < lastmod)		/* past times */
    return -1;
  age = (reftime - lastmod) / 3600;
  if (age < 48)				/* last 2 days: 1 hour resolution */
    return age;
  age = (age-48) / 24;
  if (age < 64)				/* next 64 days: 1 day resolution */
    return 48 + age;
  age = (age-64) / 7;
  if (age < 135)			/* next 135 weeks: 1 week resolution */
    return 112 + age;
  age = (age-135) / 52;
  if (age < 8)				/* next 8 years: 1 year resolution */
    return 247 + age;
  return 255;				/* then just "infinite future" */
}

#endif
