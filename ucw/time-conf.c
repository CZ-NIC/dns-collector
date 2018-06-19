/*
 *	UCW Library -- A Parser of Time Intervals
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/time.h>
#include <ucw/fastbuf.h>

static char *
timestamp_parser(char *c, void *ptr)
{
  timestamp_t *ts = ptr;
  double d;

  char *err = cf_parse_double(c, &d);
  if (err)
    return err;
  if (d > 1e16)
    return "Time too large";

  *ts = (timestamp_t)(d*1000 + 0.5);
  if (!*ts && d)
    return "Non-zero time rounds down to zero milliseconds";

  return NULL;
}

static void
timestamp_dumper(struct fastbuf *fb, void *ptr)
{
  timestamp_t *ts = ptr;
  bprintf(fb, "%jd", (intmax_t) *ts);
}

struct cf_user_type timestamp_type = {
  .size = sizeof(timestamp_t),
  .name = "timestamp",
  .parser = timestamp_parser,
  .dumper = timestamp_dumper
};

#ifdef TEST

#include <stdio.h>

int main(void)
{
  char line[256];

  while (fgets(line, sizeof(line), stdin))
    {
      char *nl = strchr(line, '\n');
      if (nl)
	*nl = 0;
      timestamp_t t = -1;
      char *err = timestamp_parser(line, &t);
      if (err)
	puts(err);
      else
	printf("%jd\n", (intmax_t) t);
    }
  return 0;
}

#endif
