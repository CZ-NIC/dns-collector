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
stk_array_copy(char *x, char **s, uns cnt)
{
  while (cnt--)
    {
      uns l = strlen(*s);
      memcpy(x, *s, l);
      x += l;
      s++;
    }
  *x = 0;
}

char *stk_printf_buf;
static int stk_printf_len;

uns
stk_printf_internal(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  if (!stk_printf_buf)
    {
      stk_printf_buf = xmalloc(256);
      stk_printf_len = 256;
    }
  for (;;)
    {
      int l = vsnprintf(stk_printf_buf, stk_printf_len, fmt, args);
      if (l < 0)
	stk_printf_len *= 2;
      else if (l < stk_printf_len)
	return l+1;
      else
	stk_printf_len = MAX(stk_printf_len*2, l+1);
      stk_printf_buf = xrealloc(stk_printf_buf, stk_printf_len);
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
  char *a = stk_strdup("are");
  a = stk_strcat(a, " the ");
  a = stk_strmulticat(a, "Jabberwock, ", "my", NULL);
  char *arr[] = { a, " son" };
  a = stk_strarraycat(arr, 2);
  a = stk_printf("Bew%s!", a);
  puts(a);
  puts(stk_hexdump(a, 3));
  return 0;
}

#endif
