#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <lib/lib.h>
#include <charset/unicode.h>

int main(void)
{
  byte buf[256];
  byte *c;
  word u[256], *w;

  while (fgets(buf, sizeof(buf), stdin))
    {
      if (c = strchr(buf, '\n'))
	*c = 0;
      utf8_to_ucs2(u, buf);
      ucs2_to_utf8(buf, u);
      puts(buf);
      c = buf;
      for(w=u; *w; w++)
	*c++ = Usig(*w);
      *c = 0;
      puts(buf);
      for(w=u; *w; w++)
	*w = Uunaccent(*w);
      ucs2_to_utf8(buf, u);
      puts(buf);
      for(w=u; *w; w++)
	*w = Utoupper(*w);
      ucs2_to_utf8(buf, u);
      puts(buf);
      for(w=u; *w; w++)
	*w = Utolower(*w);
      ucs2_to_utf8(buf, u);
      puts(buf);
      for(w=u; *w; w++)
	if (!Cprint(*w))
	  putchar('?');
	else if (Cdigit(*w))
	  putchar('0');
	else if (Clower(*w))
	  putchar('a');
	else if (Cupper(*w))
	  putchar('A');
	else if (Cblank(*w))
	  putchar('_');
	else
	  putchar('.');
      putchar('\n');
    }

  return 0;
}
