/*
 *	Sherlock Gatherer: Data structures used in indices
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

/* Words */

#define MAX_WORD_LEN		64

/* Word types */

enum word_type {
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
