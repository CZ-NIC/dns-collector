/*
 *	Sherlock Library -- auto-resizable printf() functions
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/printf.h"

#include <stdio.h>

byte *
vxprintf(char *msg, va_list v)
{
	static byte *buf = NULL;
	static int buf_len = 0;
	int len;
	if (!buf)
	{
		buf_len = 1024;
		buf = xmalloc(buf_len);
	}
	while (1)
	{
		len = vsnprintf(buf, buf_len, msg, v);
		if (len >= 0 && len < buf_len)
			return buf;
		else
		{
			buf_len *= 2;
			if (len >= buf_len)
				buf_len = len + 1;
			buf = xrealloc(buf, buf_len);
		}
	}
}

byte *
xprintf(char *msg, ...)
{
	byte *txt;
	va_list v;
	va_start(v, msg);
	txt = vxprintf(msg, v);
	va_end(v);
	return txt;
}
