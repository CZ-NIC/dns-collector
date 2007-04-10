#include "lib/lib.h"
#include "lib/stkstring.h"

#include <stdio.h>

uns
stk_array_len(char **s, uns cnt)
{
  uns l = 1;
  while (cnt--)
    l += strlen(*s++);
  return l;
}

void
stk_array_join(char *x, char **s, uns cnt, uns sep)
{
  while (cnt--)
    {
      uns l = strlen(*s);
      memcpy(x, *s, l);
      x += l;
      s++;
      if (sep && cnt)
	*x++ = sep;
    }
  *x = 0;
}

uns
stk_printf_internal(const char *fmt, ...)
{
  uns len = 256;
  char *buf = alloca(len);
  va_list args, args2;
  va_start(args, fmt);
  for (;;)
    {
      va_copy(args2, args);
      int l = vsnprintf(buf, len, fmt, args2);
      va_end(args2);
      if (l < 0)
	len *= 2;
      else
	{
	  va_end(args);
	  return l+1;
	}
      buf = alloca(len);
    }
}

uns
stk_vprintf_internal(const char *fmt, va_list args)
{
  uns len = 256;
  char *buf = alloca(len);
  va_list args2;
  for (;;)
    {
      va_copy(args2, args);
      int l = vsnprintf(buf, len, fmt, args2);
      va_end(args2);
      if (l < 0)
	len *= 2;
      else
	{
	  va_end(args);
	  return l+1;
	}
      buf = alloca(len);
    }
}

void
stk_hexdump_internal(char *dst, byte *src, uns n)
{
  for (uns i=0; i<n; i++)
    {
      if (i)
	*dst++ = ' ';
      dst += sprintf(dst, "%02x", *src++);
    }
  *dst = 0;
}

#ifdef TEST

int main(void)
{
  char *a = stk_strndup("are!",3);
  a = stk_strcat(a, " the ");
  a = stk_strmulticat(a, stk_strdup("Jabberwock, "), "my", NULL);
  char *arr[] = { a, " son" };
  a = stk_strarraycat(arr, 2);
  a = stk_printf("Bew%s!", a);
  puts(a);
  puts(stk_hexdump(a, 3));
  char *ary[] = { "The", "jaws", "that", "bite" };
  puts(stk_strjoin(ary, 4, ' '));
  return 0;
}

#endif
