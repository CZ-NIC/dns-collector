/*
 *	UCW Library -- A simple growing buffer
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	Define the following macros:
 *
 *	GBUF_TYPE	data type of records stored in the buffer
 *	GBUF_PREFIX(x)	add a name prefix to all global symbols
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <stdlib.h>

#define	BUF_T	GBUF_PREFIX(t)

typedef struct
{
  uns len;
  GBUF_TYPE *ptr;
}
BUF_T;

static inline void
GBUF_PREFIX(init)(BUF_T *b)
{
  b->ptr = NULL;
  b->len = 0;
}

static inline void
GBUF_PREFIX(done)(BUF_T *b)
{
  if (b->ptr)
    xfree(b->ptr);
  b->ptr = NULL;
  b->len = 0;
}

static inline void
GBUF_PREFIX(realloc)(BUF_T *b, uns len)
{
  b->len = len;
  b->ptr = xrealloc(b->ptr, len * sizeof(GBUF_TYPE));
}

static inline void
GBUF_PREFIX(grow)(BUF_T *b, uns len)
{
  if (len <= b->len)
    return;
  if (len < 2*b->len)			// to ensure logarithmic cost
    len = 2*b->len;
  GBUF_PREFIX(realloc)(b, len);
}

#undef	GBUF_TYPE
#undef	GBUF_PREFIX
#undef	BUF_T
