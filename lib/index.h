/*
 *	Sherlock: Data structures used in indices
 *
 *	(c) 2001--2003 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_INDEX_H
#define _SHERLOCK_INDEX_H

#include "lib/fastbuf.h"
#include SHERLOCK_CUSTOM
#include "charset/unistream.h"

#define INDEX_VERSION (0x32240100+sizeof(struct card_attr))	/* Increase with each incompatible change in index format */

/*
 *  Words and word complexes
 *
 *  MAX_WORD_LEN is the maximum length (measured in UTF-8 characters, excluding
 *  the terminating zero byte if there's any) of any word which may appear in the
 *  indices or in the bucket file. Naturally, the same constant also bounds
 *  the number of UCS-2 characters in a word.
 *
 *  Caveat: If you are upcasing/downcasing the word, the UTF-8 encoding can
 *  expand, although at most twice, so you need to reserve 2*MAX_WORD_LEN bytes.
 *
 *  MAX_COMPLEX_LEN is the upper bound on number of words in any word complex.
 */

#define MAX_WORD_LEN		64	/* a multiple of 4 */
#define MAX_COMPLEX_LEN		10

/* Word and string types are defined in lib/custom.h */

/* Index card attributes */

struct card_attr {
  u32 card;				/* Reference to card description (either oid or filepos) */
#ifdef CONFIG_SITES
  u32 site_id;
#endif
  CUSTOM_CARD_ATTRS			/* Include all custom attributes */
  byte weight;
  byte flags;
#ifdef CONFIG_LASTMOD
  byte age;				/* Document age in pseudo-logarithmic units wrt. reference time */
#endif
#ifdef CONFIG_FILETYPE
  byte type_flags;			/* File type flags (see below) */
#endif
};

enum card_flag {
  CARD_FLAG_EMPTY = 1,			/* Empty document (redirect, robot file etc.) [scanner] */
  CARD_FLAG_ACCENTED = 2,		/* Document contains accented characters [scanner] */
  CARD_FLAG_DUP = 4,			/* Removed as a duplicate [merger] */
  CARD_FLAG_MERGED = 8,			/* Destination of a merge [merger] */
  CARD_FLAG_IMAGE = 16,			/* Is an image object [scanner] */
  CARD_FLAG_FRAMESET = 32,		/* Contains a frameset to be ignored [scanner] */
  CARD_FLAG_GIANT_CLASS = 64,		/* Belongs to a very large class, subject to penalties [merger] */
};

#define CARD_POS_SHIFT 5		/* Card positions are shifted this # of bits to the right */

/*
 *  We store document type and several other properties in card_attr->type_flags.
 *  Here we define only the basic structure, the details are defined in custom.h
 *  (the list of type names custom_file_type_names[] and also setting of the file
 *  types in custom_create_attrs()).
 *
 *  bits 7--5	file type: (0-3: text types, 4-7: other types, defined by custom.h)
 *  bits 4--0	type-dependent information, for text types it's document language code
 */

#ifdef CONFIG_FILETYPE
#define CA_GET_FILE_TYPE(a) ((a)->type_flags >> 5)
#define CA_GET_FILE_INFO(a) ((a)->type_flags & 0x1f)
#define CA_GET_FILE_LANG(a) ((a)->type_flags & 0x80 ? 0 : CA_GET_FILE_INFO(a))
#define FILETYPE_ATTRS SMALL_SET_ATTR(ftype, FILETYPE, CA_GET_FILE_TYPE, ext_ft_parse)
byte *ext_ft_parse(u32 *dest, byte *value, uns intval);
extern byte *custom_file_type_names[8];
#else
#define FILETYPE_ATTRS
#endif

#ifdef CONFIG_LANG
/* You can use language matching without CONFIG_FILETYPE, but you have to define CA_GET_FILE_LANG yourself. */
#define LANG_ATTRS SMALL_SET_ATTR(lang, LANG, CA_GET_FILE_LANG, ext_lang_parse)
byte *ext_lang_parse(u32 *dest, byte *value, uns intval);
#else
#define LANG_ATTRS
#endif

#define EXTENDED_ATTRS CUSTOM_ATTRS FILETYPE_ATTRS LANG_ATTRS

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

#define SKIP_TAGGED_CHAR(p) do {				\
  if (*p >= 0x80 && *p < 0xc0)					\
    {								\
      uns u = *p++;						\
      if (u >= 0xa0 && u < 0xb0 && *p >= 0x80 && *p < 0xc0)	\
	p++;							\
    }								\
  else								\
    UTF8_SKIP(p);						\
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
