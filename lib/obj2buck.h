/*
 *	Generating V33 buckets
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

#define	WRITE_V33(ptr, type, text, len)	({\
  uns _len = len;		\
  PUT_UTF8(ptr, _len+1);	\
  memcpy(ptr, text, _len);	\
  ptr += _len;			\
  *ptr++ = type;		\
})

#define	PUTS_V33(ptr, type, text)	WRITE_V33(ptr, type, text, strlen(text))

#define	VPRINTF_V33(ptr, type, mask, va)	({\
  uns _len = vsprintf(ptr+1, mask, va) + 1;	\
  *ptr = _len;			\
  ptr += _len;			\
  *ptr++ = type;		\
})	// requires _len < 127 !

#define	PRINTF_V33(ptr, type, mask...)	({\
  uns _len = sprintf(ptr+1, mask) + 1;	\
  *ptr = _len;			\
  ptr += _len;			\
  *ptr++ = type;		\
})	// requires _len < 127 !

#define	PUTL_V33(ptr, type, num)	PRINTF_V33(ptr, type, "%d", num)

#include "charset/unistream.h"

static inline void
bwrite_v33(struct fastbuf *b, uns type, byte *text, uns len)
{
  bput_utf8(b, len+1);
  bwrite(b, text, len);
  bputc(b, type);
}

static inline void
bputs_v33(struct fastbuf *b, uns type, byte *text)
{
  bwrite_v33(b, type, text, strlen(text));
}

#include <stdarg.h>

static void UNUSED
bprintf_v33(struct fastbuf *b, uns type, byte *mask, ...)
  /* requires _len < 127 ! */
{
  byte *ptr;
  if (bdirect_write_prepare(b, &ptr) < 130)
  {
    bflush(b);
    bdirect_write_prepare(b, &ptr);
  }
  va_list va;
  va_start(va, mask);
  VPRINTF_V33(ptr, type, mask, va);
  bdirect_write_commit(b, ptr);
}

static inline void
bputl_v33(struct fastbuf *b, uns type, uns num)
{
  byte *ptr;
  if (bdirect_write_prepare(b, &ptr) < 20)
  {
    bflush(b);
    bdirect_write_prepare(b, &ptr);
  }
  PUTL_V33(ptr, type, num);
  bdirect_write_commit(b, ptr);
}
