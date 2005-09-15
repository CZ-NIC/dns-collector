/*
 *	UCW Library -- A simple growing buffer
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *	(c) 2005, Martin Mares <mj@ucw.cz>
 *
 *	Define the following macros:
 *
 *	GBUF_TYPE	data type of records stored in the buffer
 *	GBUF_PREFIX(x)	add a name prefix to all global symbols
 *	GBUF_TRACE(msg...) log growing of buffer [optional]
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

static void UNUSED
GBUF_PREFIX(done)(BUF_T *b)
{
  if (b->ptr)
    xfree(b->ptr);
  b->ptr = NULL;
  b->len = 0;
}

static void UNUSED
GBUF_PREFIX(set_size)(BUF_T *b, uns len)
{
  b->len = len;
  b->ptr = xrealloc(b->ptr, len * sizeof(GBUF_TYPE));
#ifdef GBUF_TRACE
  GBUF_TRACE(STRINGIFY_EXPANDED(BUF_T) " growing to %u items", len);
#endif
}

static void UNUSED
GBUF_PREFIX(do_grow)(BUF_T *b, uns len)
{
  if (len < 2*b->len)			// to ensure logarithmic cost
    len = 2*b->len;
  GBUF_PREFIX(set_size)(b, len);
}

static inline GBUF_TYPE *
GBUF_PREFIX(grow)(BUF_T *b, uns len)
{
  if (unlikely(len > b->len))
    GBUF_PREFIX(do_grow)(b, len);
  return b->ptr;
}

#undef	GBUF_TYPE
#undef	GBUF_PREFIX
#undef  GBUF_TRACE
#undef	BUF_T
