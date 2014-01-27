/*
 *	UCW Library -- Formatting of command-line option help
 *
 *	(c) 2013 Jan Moskyto Matejka <mq@ucw.cz>
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/opt.h>
#include <ucw/opt-internal.h>
#include <ucw/mempool.h>
#include <ucw/gary.h>

#include <string.h>

struct help {
  struct mempool *pool;
  struct help_line *lines;			// A growing array of lines
};

struct help_line {
  const char *extra;
  char *fields[3];
};

static void opt_help_scan_item(struct help *h, struct opt_precomputed *opt)
{
  struct opt_item *item = opt->item;

  if (opt->flags & OPT_NO_HELP)
    return;

  if (item->cls == OPT_CL_HELP) {
    struct help_line *l = GARY_PUSH(h->lines, 1);
    l->extra = item->help ? : "";
    return;
  }

  if (item->letter >= OPT_POSITIONAL_TAIL)
    return;

  struct help_line *first = GARY_PUSH(h->lines, 1);
  if (item->help) {
    char *text = mp_strdup(h->pool, item->help);
    struct help_line *l = first;
    while (text) {
      char *eol = strchr(text, '\n');
      if (eol)
	*eol++ = 0;

      int field = (l == first ? 1 : 0);
      char *f = text;
      while (f) {
	char *tab = strchr(f, '\t');
	if (tab)
	  *tab++ = 0;
	if (field < 3)
	  l->fields[field++] = f;
	f = tab;
      }

      text = eol;
      if (text)
	l = GARY_PUSH(h->lines, 1);
    }
  }

  if (item->name) {
    char *val = first->fields[1] ? : "";
    if (opt->flags & OPT_REQUIRED_VALUE)
      val = mp_printf(h->pool, "=%s", val);
    else if (!(opt->flags & OPT_NO_VALUE))
      val = mp_printf(h->pool, "[=%s]", val);
    first->fields[1] = mp_printf(h->pool, "--%s%s", item->name, val);
  }

  if (item->letter) {
    if (item->name)
      first->fields[0] = mp_printf(h->pool, "-%c, ", item->letter);
    else {
      char *val = first->fields[1] ? : "";
      if (!(opt->flags & OPT_REQUIRED_VALUE) && !(opt->flags & OPT_NO_VALUE))
	val = mp_printf(h->pool, "[%s]", val);
      first->fields[0] = mp_printf(h->pool, "-%c%s", item->letter, val);
      first->fields[1] = NULL;
    }
  }
}

static void opt_help_scan(struct help *h, const struct opt_section *sec)
{
  for (struct opt_item * item = sec->opt; item->cls != OPT_CL_END; item++) {
    if (item->cls == OPT_CL_SECTION)
      opt_help_scan(h, item->u.section);
    else {
      struct opt_precomputed opt;
      opt_precompute(&opt, item);
      opt_help_scan_item(h, &opt);
    }
  }
}

void opt_help(const struct opt_section * sec) {
  // Prepare help text
  struct help h;
  h.pool = mp_new(4096);
  GARY_INIT_ZERO(h.lines, 0);
  opt_help_scan(&h, sec);

  // Calculate natural width of each column
  uns n = GARY_SIZE(h.lines);
  uns widths[3] = { 0, 0, 0 };
  for (uns i=0; i<n; i++) {
    struct help_line *l = &h.lines[i];
    for (uns f=0; f<3; f++) {
      if (!l->fields[f])
	l->fields[f] = "";
      uns w = strlen(l->fields[f]);
      widths[f] = MAX(widths[f], w);
    }
  }
  if (widths[0] > 4) {
    /*
     *  This is tricky: if there are short options, which have an argument,
     *  but no long variant, we are willing to let column 0 overflow to column 1.
     */
    widths[1] = MAX(widths[1], widths[0] - 4);
    widths[0] = 4;
  }
  widths[1] += 4;

  // Print columns
  for (uns i=0; i<n; i++) {
    struct help_line *l = &h.lines[i];
    if (l->extra)
      puts(l->extra);
    else {
      int t = 0;
      for (uns f=0; f<3; f++) {
	t += widths[f];
	t -= printf("%s", l->fields[f]);
	while (t > 0) {
	  putchar(' ');
	  t--;
	}
      }
      putchar('\n');
    }
  }

  // Clean up
  GARY_FREE(h.lines);
  mp_delete(h.pool);
}

