/*
 *	Sherlock: Data structures used in indices
 *
 *	(c) 2001--2004 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_INDEX_H
#define _SHERLOCK_INDEX_H

#include "custom/lib/custom.h"

/*
 *  Magic number which should help to avoid mixing incompatible indices.
 *  Syntax: <magic-byte><version><revision><custom-dependent-part>
 *  Remember to increase with each change of index format.
 */

#ifndef CUSTOM_INDEX_VERSION
#define CUSTOM_INDEX_VERSION sizeof(struct card_attr)
#endif

#define INDEX_VERSION (0x32340100+CUSTOM_INDEX_VERSION)

/*
 *  Words
 *
 *  MAX_WORD_LEN is the maximum length (measured in UTF-8 characters, excluding
 *  the terminating zero byte if there's any) of any word which may appear in the
 *  indices or in the bucket file. Naturally, the same constant also bounds
 *  the number of UCS-2 characters in a word.
 *
 *  Caveat: If you are upcasing/downcasing the word, the UTF-8 encoding can
 *  expand, although at most twice, so you need to reserve 2*MAX_WORD_LEN bytes.
 */

#define MAX_WORD_LEN		64	/* a multiple of 4 */

/* Word and string types are defined in custom/lib/custom.h */

/* Types used for storing contexts */

#ifdef CONFIG_CONTEXTS
#if CONFIG_MAX_CONTEXTS == 32768
typedef u16 context_t;
#define bget_context bgetw
#define bput_context bputw
#define GET_CONTEXT GET_U16
#define PUT_CONTEXT PUT_U16
#elif CONFIG_MAX_CONTEXTS == 256
typedef byte context_t;
#define bget_context bgetc
#define bput_context bputc
#define GET_CONTEXT GET_U8
#define PUT_CONTEXT PUT_U8
#else
#error CONFIG_MAX_CONTEXTS set to an invalid value.
#endif
#else
struct fastbuf;
typedef struct { } context_t;
static inline uns bget_context(struct fastbuf *b UNUSED) { return 0; }
static inline void bput_context(struct fastbuf *b UNUSED, uns context UNUSED) { }
#define GET_CONTEXT(p) 0
#define PUT_CONTEXT(p,x) do {} while(0)
#endif

/* Index card attributes */

struct card_attr {
  u32 card;				/* Reference to card description (either oid or filepos) */
#ifdef CONFIG_SITES
  u32 site_id;
#endif
  area_t area;
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
  CARD_FLAG_OVERRIDEN = 64,		/* Overriden by another index [sherlockd] */
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
#define CA_GET_FILE_TYPE(a) ((a)->type_flags >> 4)
#define CA_GET_FILE_INFO(a) ((a)->type_flags & 0x0f)
#define CA_GET_FILE_LANG(a) ((a)->type_flags & 0x80 ? 0 : CA_GET_FILE_INFO(a))
#define MAX_FILE_TYPES 16
#define FILETYPE_IS_TEXT(f) ((f) < 8)
byte *ext_ft_parse(u32 *dest, byte *value, uns intval);
extern byte *custom_file_type_names[MAX_FILE_TYPES];
#define FILETYPE_STAT_VARS uns matching_per_type[MAX_FILE_TYPES];
#define FILETYPE_SHOW_STATS(q,f) ext_ft_show(q,f)
#define FILETYPE_INIT_STATS(q) bzero(q->matching_per_type, sizeof(q->matching_per_type))
#ifdef CONFIG_COUNT_ALL_FILETYPES
#define FILETYPE_ATTRS LATE_SMALL_SET_ATTR(ftype, FILETYPE, CA_GET_FILE_TYPE, ext_ft_parse)
#define FILETYPE_EARLY_STATS(q,a) q->matching_per_type[CA_GET_FILE_TYPE(a)]++
#define FILETYPE_LATE_STATS(q,a)
#else
#define FILETYPE_ATTRS SMALL_SET_ATTR(ftype, FILETYPE, CA_GET_FILE_TYPE, ext_ft_parse)
#define FILETYPE_EARLY_STATS(q,a)
#define FILETYPE_LATE_STATS(q,a) q->matching_per_type[CA_GET_FILE_TYPE(a)]++
#endif
#else
#define FILETYPE_ATTRS
#define FILETYPE_STAT_VARS
#define FILETYPE_INIT_STATS(q)
#define FILETYPE_EARLY_STATS(q,a)
#define FILETYPE_LATE_STATS(q,a)
#define FILETYPE_SHOW_STATS(q,f)
#endif

#ifdef CONFIG_LANG
/* You can use language matching without CONFIG_FILETYPE, but you have to define CA_GET_FILE_LANG yourself. */
#define LANG_ATTRS SMALL_SET_ATTR(lang, LANG, CA_GET_FILE_LANG, ext_lang_parse)
byte *ext_lang_parse(u32 *dest, byte *value, uns intval);
#else
#define LANG_ATTRS
#endif

#ifdef CONFIG_AREAS
#define CA_GET_AREA(a) ((a)->area)
#define SPLIT_ATTRS INT_ATTR(area, AREA, CA_GET_AREA, ext_area_parse)
byte *ext_area_parse(u32 *dest, byte *value, uns intval);
#else
#define SPLIT_ATTRS
#endif

/*
 * A list of all extended attributes: custom attributes and also some
 * built-in attributes treated in the same way.
 */

#define EXTENDED_ATTRS CUSTOM_ATTRS FILETYPE_ATTRS LANG_ATTRS SPLIT_ATTRS

/*
 * A list of all statistics collectors, also composed of custom parts
 * and built-in parts.
 */

#ifndef CUSTOM_STAT_VARS
#define CUSTOM_STAT_VARS
#define CUSTOM_INIT_STATS(q)
#define CUSTOM_EARLY_STATS(q,a)
#define CUSTOM_LATE_STATS(q,a)
#define CUSTOM_SHOW_STATS(q,f)
#endif

#define EXTENDED_STAT_VARS CUSTOM_STAT_VARS FILETYPE_STAT_VARS
#define EXTENDED_INIT_STATS(q) CUSTOM_INIT_STATS(q) FILETYPE_INIT_STATS(q)
#define EXTENDED_EARLY_STATS(q,a) CUSTOM_EARLY_STATS(q,a) FILETYPE_EARLY_STATS(q,a)
#define EXTENDED_LATE_STATS(q,a) CUSTOM_LATE_STATS(q,a) FILETYPE_LATE_STATS(q,a)
#define EXTENDED_SHOW_STATS(q,f) CUSTOM_SHOW_STATS(q,f) FILETYPE_SHOW_STATS(q,f)

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

/* The card fingerprints */

struct card_print {
  struct fingerprint fp;
  u32 cardid;
};

/* URL keys */

#define URL_KEY_BUF_SIZE (3*MAX_URL_SIZE)
byte *url_key(byte *url, byte *buf);
void url_fingerprint(byte *url, struct fingerprint *fp);
void url_key_init(void);

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
