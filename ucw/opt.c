/*
 *	UCW Library -- Parsing of command line options
 *
 *	(c) 2013 Jan Moskyto Matejka <mq@ucw.cz>
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/opt.h>
#include <ucw/conf.h>
#include <ucw/conf-internal.h>
#include <ucw/fastbuf.h>
#include <ucw/stkstring.h>
#include <ucw/strtonum.h>
#include <ucw/mempool.h>
#include <ucw/gary.h>

#include <alloca.h>
#include <math.h>

/***
 * Value flags defaults
 * ~~~~~~~~~~~~~~~~~~~~
 *
 * OPT_NO_VALUE for OPT_BOOL, OPT_SWITCH and OPT_INC
 * OPT_MAYBE_VALUE for OPT_STRING, OPT_UNS, OPT_INT
 * Some of the value flags (OPT_NO_VALUE, OPT_MAYBE_VALUE, OPT_REQUIRED_VALUE)
 * must be specified for OPT_CALL and OPT_USER.
 ***/
static uns opt_default_value_flags[] = {
    [OPT_CL_BOOL] = OPT_NO_VALUE,
    [OPT_CL_STATIC] = OPT_MAYBE_VALUE,
    [OPT_CL_SWITCH] = OPT_NO_VALUE,
    [OPT_CL_INC] = OPT_NO_VALUE,
    [OPT_CL_CALL] = 0,
    [OPT_CL_USER] = 0,
    [OPT_CL_SECTION] = 0,
    [OPT_CL_HELP] = 0
};

struct opt_precomputed {
  struct opt_item * item;
  const char * name;
  short flags;
  short count;
};

struct opt_context {
  struct opt_precomputed * opts;
  struct opt_precomputed ** shortopt;
  struct opt_item ** hooks_before_arg;
  struct opt_item ** hooks_before_value;
  struct opt_item ** hooks_after_value;
  short opt_count;
  short hooks_before_arg_count;
  short hooks_before_value_count;
  short hooks_after_value_count;
  int positional_max;
  int positional_count;
};

static void opt_failure(const char * mesg, ...) FORMAT_CHECK(printf,1,2) NONRET;
static void opt_failure(const char * mesg, ...) {
  va_list args;
  va_start(args, mesg);
  vfprintf(stderr, mesg, args);
  fprintf(stderr, "\n");
  opt_usage();
  exit(OPT_EXIT_BAD_ARGS);
}

static char *opt_name(struct opt_context *oc, struct opt_precomputed *opt)
{
  struct opt_item *item = opt->item;
  char *res;
  if (item->letter >= OPT_POSITIONAL_TAIL)
    res = stk_printf("positional argument #%d", oc->positional_count);
  else if (opt->flags & OPT_SEEN_AS_LONG)
    res = stk_printf("--%s", opt->name);
  else
    res = stk_printf("-%c", item->letter);
  return xstrdup(res);
}

#define THIS_OPT opt_name(oc, opt)

static void opt_precompute(struct opt_precomputed *opt, struct opt_item *item)
{
  opt->item = item;
  opt->count = 0;
  opt->name = item->name;
  uns flags = item->flags;

  if (item->letter >= OPT_POSITIONAL_TAIL) {
    flags &= ~OPT_VALUE_FLAGS;
    flags |= OPT_REQUIRED_VALUE;
  }
  if (!(flags & OPT_VALUE_FLAGS)) {
    ASSERT(item->cls != OPT_CL_CALL && item->cls != OPT_CL_USER);
    flags |= opt_default_value_flags[item->cls];
  }

  opt->flags = flags;
}

#define FOREACHLINE(text) for (const char * begin = (text), * end = (text); (*end) && (end = strchrnul(begin, '\n')); begin = end+1)

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

static struct opt_precomputed * opt_find_item_longopt(struct opt_context * oc, char * str) {
  uns len = strlen(str);
  struct opt_precomputed * candidate = NULL;

  for (int i = 0; i < oc->opt_count; i++) {
    struct opt_precomputed *opt = &oc->opts[i];
    if (!opt->name)
      continue;

    if (!strncmp(opt->name, str, len)) {
      if (strlen(opt->name) == len)
	return opt;
    } else if (opt->item->cls == OPT_CL_BOOL && !strncmp("no-", str, 3) && !strncmp(opt->name, str+3, len-3)) {
      if (strlen(opt->name) == len-3)
	return opt;
    } else
      continue;

    if (candidate)
      opt_failure("Ambiguous option --%s: matches both --%s and --%s.", str, candidate->name, opt->name);
    else
      candidate = opt;
  }

  if (candidate)
    return candidate;

  opt_failure("Invalid option --%s.", str);
}

// FIXME: Use simple-lists?
#define OPT_PTR(type) ({ 			\
  type * ptr; 					\
  if (item->flags & OPT_MULTIPLE) { 		\
    struct { 					\
      cnode n; 					\
      type v; 					\
    } * n = xmalloc(sizeof(*n)); 		\
    clist_add_tail(item->ptr, &(n->n)); 	\
    ptr = &(n->v); 				\
  } else 					\
    ptr = item->ptr; 				\
  ptr; })

static void opt_parse_value(struct opt_context * oc, struct opt_precomputed * opt, char * value) {
  struct opt_item * item = opt->item;

  if (opt->count++ && (opt->flags & OPT_SINGLE))
    opt_failure("Option %s must be specified at most once.", THIS_OPT);

  for (int i = 0; i < oc->hooks_before_value_count; i++)
    oc->hooks_before_value[i]->u.call(item, value, oc->hooks_before_value[i]->ptr);

  switch (item->cls) {
    case OPT_CL_BOOL:
      if (!value || !strcasecmp(value, "y") || !strcasecmp(value, "yes") || !strcasecmp(value, "true") || !strcasecmp(value, "1"))
	*((int *) item->ptr) = 1 ^ (!!(opt->flags & OPT_NEGATIVE));
      else if (!strcasecmp(value, "n") || !strcasecmp(value, "no") || !strcasecmp(value, "false") || !strcasecmp(value, "0"))
	*((int *) item->ptr) = 0 ^ (!!(opt->flags & OPT_NEGATIVE));
      else
	opt_failure("Boolean argument for %s has a strange value. Supported (case insensitive): 1/0, y/n, yes/no, true/false.", THIS_OPT);
      break;
    case OPT_CL_STATIC:
      {
	char * e = NULL;
	switch (item->type) {
	  case CT_INT:
	    if (!value)
	      *OPT_PTR(int) = 0;
	    else
	      e = cf_parse_int(value, OPT_PTR(int));
	    if (e)
	      opt_failure("Integer value parsing failed for %s: %s", THIS_OPT, e);
	    break;
	  case CT_U64:
	    if (!value)
	      *OPT_PTR(u64) = 0;
	    else
	      e = cf_parse_u64(value, OPT_PTR(u64));
	    if (e)
	      opt_failure("Unsigned 64-bit value parsing failed for %s: %s", THIS_OPT, e);
	    break;
	  case CT_DOUBLE:
	    if (!value)
	      *OPT_PTR(double) = NAN;
	    else
	      e = cf_parse_double(value, OPT_PTR(double));
	    if (e)
	      opt_failure("Floating-point value parsing failed for %s: %s", THIS_OPT, e);
	    break;
	  case CT_IP:
	    if (!value)
	      *OPT_PTR(u32) = 0;
	    else
	      e = cf_parse_ip(value, OPT_PTR(u32));
	    if (e)
	      opt_failure("IP address parsing failed for %s: %s", THIS_OPT, e);
	    break;
	  case CT_STRING:
	    if (!value)
	      *OPT_PTR(const char *) = NULL;
	    else
	      *OPT_PTR(const char *) = xstrdup(value);
	    break;
	  default:
	    ASSERT(0);
	}
	break;
      }
    case OPT_CL_SWITCH:
      if ((opt->flags & OPT_SINGLE) && *((int *)item->ptr) != -1)
	opt_failure("Multiple switches: %s", THIS_OPT);
      else
	*((int *)item->ptr) = item->u.value;
      break;
    case OPT_CL_INC:
      if (opt->flags & OPT_NEGATIVE)
	(*((int *)item->ptr))--;
      else
	(*((int *)item->ptr))++;
      break;
    case OPT_CL_CALL:
      item->u.call(item, value, item->ptr);
      break;
    case OPT_CL_USER:
      {
	char * e = NULL;
	e = item->u.utype->parser(value, OPT_PTR(void*));
	if (e)
	  opt_failure("Cannot parse the value of %s: %s", THIS_OPT, e);
	break;
      }
    default:
      ASSERT(0);
  }

  for (int i = 0;i < oc->hooks_after_value_count; i++)
    oc->hooks_after_value[i]->u.call(item, value, oc->hooks_after_value[i]->ptr);
}

static int opt_longopt(struct opt_context * oc, char ** argv, int index) {
  int eaten = 0;
  char * name_in = argv[index] + 2; // skipping the -- on the beginning
  uns pos = strchrnul(name_in, '=') - name_in;
  struct opt_precomputed * opt = opt_find_item_longopt(oc, strndupa(name_in, pos));
  char * value = NULL;

  opt->flags |= OPT_SEEN_AS_LONG;

  if (opt->item->cls == OPT_CL_BOOL && !strncmp(name_in, "no-", 3) && !strncmp(name_in+3, opt->item->name, pos-3)) {
    if (name_in[pos])
      opt_failure("Option --%s must not have any value.", name_in);
    value = "n";
  } else if (opt->flags & OPT_REQUIRED_VALUE) {
    if (name_in[pos])
      value = name_in + pos + 1;
    else {
      value = argv[index+1];
      if (!value)
	opt_failure("Option %s must have a value, but nothing supplied.", THIS_OPT);
      eaten++;
    }
  } else if (opt->flags & OPT_MAYBE_VALUE) {
    if (name_in[pos])
      value = name_in + pos + 1;
  } else {
    if (name_in[pos])
      opt_failure("Option %s must have no value.", THIS_OPT);
  }
  opt_parse_value(oc, opt, value);
  return eaten;
}

static int opt_shortopt(struct opt_context * oc, char ** argv, int index) {
  int chr = 0;
  struct opt_precomputed * opt;
  int o;

  while (o = argv[index][++chr]) {
    if (o < 0 || o >= 128)
      opt_failure("Invalid character 0x%02x in option name. Only ASCII is allowed.", o & 0xff);
    opt = oc->shortopt[o];

    if (!opt)
      opt_failure("Unknown option -%c.", o);

    opt->flags &= ~OPT_SEEN_AS_LONG;

    if (opt->flags & OPT_NO_VALUE)
      opt_parse_value(oc, opt, NULL);
    else if (opt->flags & OPT_REQUIRED_VALUE) {
      if (argv[index][chr+1]) {
        opt_parse_value(oc, opt, argv[index] + chr + 1);
	return 0;
      } else if (!argv[index+1])
	opt_failure("Option -%c must have a value, but nothing supplied.", o);
      else {
	opt_parse_value(oc, opt, argv[index+1]);
	return 1;
      }
    } else if (opt->flags & OPT_MAYBE_VALUE) {
      if (argv[index][chr+1]) {
        opt_parse_value(oc, opt, argv[index] + chr + 1);
	return 0;
      } else
	opt_parse_value(oc, opt, NULL);
    } else {
      ASSERT(0);
    }
  }

  return 0;
}

static void opt_positional(struct opt_context * oc, char * value) {
  oc->positional_count++;
  uns id = oc->positional_count > oc->positional_max ? OPT_POSITIONAL_TAIL : OPT_POSITIONAL(oc->positional_count);
  struct opt_precomputed * opt = oc->shortopt[id];
  if (!opt)
    opt_failure("Too many positional arguments.");
  else {
    opt->flags &= OPT_SEEN_AS_LONG;
    opt_parse_value(oc, opt, value);
  }
}

static void opt_count_items(struct opt_context *oc, const struct opt_section *sec)
{
  for (const struct opt_item *item = sec->opt; item->cls != OPT_CL_END; item++) {
    if (item->cls == OPT_CL_SECTION)
      opt_count_items(oc, item->u.section);
    else if (item->cls == OPT_CL_HOOK) {
      if (item->flags & OPT_HOOK_BEFORE_ARG)
	oc->hooks_before_arg_count++;
      else if (item->flags & OPT_HOOK_BEFORE_VALUE)
	oc->hooks_before_value_count++;
      else if (item->flags & OPT_HOOK_AFTER_VALUE)
	oc->hooks_after_value_count++;
      else
	ASSERT(0);
    } else if (item->letter || item->name) {
      oc->opt_count++;
      if (item->letter > OPT_POSITIONAL_TAIL)
	oc->positional_max++;
    }
  }
}

static void opt_prepare_items(struct opt_context *oc, const struct opt_section *sec)
{
  for (struct opt_item *item = sec->opt; item->cls != OPT_CL_END; item++) {
    if (item->cls == OPT_CL_SECTION)
      opt_prepare_items(oc, item->u.section);
    else if (item->cls == OPT_CL_HOOK) {
      if (item->flags & OPT_HOOK_BEFORE_ARG)
	oc->hooks_before_arg[oc->hooks_before_arg_count++] = item;
      else if (item->flags & OPT_HOOK_BEFORE_VALUE)
	oc->hooks_before_value[oc->hooks_before_value_count++] = item;
      else if (item->flags & OPT_HOOK_AFTER_VALUE)
	oc->hooks_after_value[oc->hooks_after_value_count++] = item;
      else
	ASSERT(0);
    } else if (item->letter || item->name) {
      struct opt_precomputed * opt = &oc->opts[oc->opt_count++];
      opt_precompute(opt, item);
      if (item->letter)
	oc->shortopt[(int) item->letter] = opt;
    }
  }
}

static void opt_check_required(struct opt_context *oc)
{
  for (int i = 0; i < oc->opt_count; i++) {
    struct opt_precomputed *opt = &oc->opts[i];
    if (!opt->count && (opt->flags & OPT_REQUIRED)) {
      struct opt_item *item = opt->item;
      if (item->letter > OPT_POSITIONAL_TAIL)
	opt_failure("Required positional argument #%d not found.", item->letter - OPT_POSITIONAL_TAIL);
      else if (item->letter == OPT_POSITIONAL_TAIL)
	opt_failure("Required positional argument not found.");
      else if (item->letter && item->name)
	opt_failure("Required option -%c/--%s not found.", item->letter, item->name);
      else if (item->letter)
	opt_failure("Required option -%c not found.", item->letter);
      else
	opt_failure("Required option --%s not found.", item->name);
    }
  }
}

void opt_parse(const struct opt_section * options, char ** argv) {
  struct opt_context * oc = alloca(sizeof(*oc));
  memset(oc, 0, sizeof (*oc));

  opt_count_items(oc, options);
  oc->opts = alloca(sizeof(*oc->opts) * oc->opt_count);
  oc->shortopt = alloca(sizeof(*oc->shortopt) * (oc->positional_max + 257));
  memset(oc->shortopt, 0, sizeof(*oc->shortopt) * (oc->positional_max + 257));
  oc->hooks_before_arg = alloca(sizeof (*oc->hooks_before_arg) * oc->hooks_before_arg_count);
  oc->hooks_before_value = alloca(sizeof (*oc->hooks_before_value) * oc->hooks_before_value_count);
  oc->hooks_after_value = alloca(sizeof (*oc->hooks_after_value) * oc->hooks_after_value_count);

  oc->opt_count = 0;
  oc->hooks_before_arg_count = 0;
  oc->hooks_before_value_count = 0;
  oc->hooks_after_value_count = 0;
  opt_prepare_items(oc, options);

  int force_positional = 0;
  for (int i = 0; argv[i]; i++) {
    char *arg = argv[i];
    for (int j = 0; j < oc->hooks_before_arg_count; j++)
      oc->hooks_before_arg[j]->u.call(NULL, NULL, oc->hooks_before_arg[j]->ptr);
    if (arg[0] != '-' || force_positional)
      opt_positional(oc, arg);
    else {
      if (arg[1] == '-') {
	if (arg[2] == '\0')
	  force_positional++;
	else
	  i += opt_longopt(oc, argv, i);
      } else if (arg[1])
	i += opt_shortopt(oc, argv, i);
      else
	opt_positional(oc, arg);
    }
  }

  opt_check_required(oc);
}

static void opt_conf_end_of_options(struct cf_context *cc) {
  cf_load_default(cc);
  if (cc->postpone_commit && cf_close_group())
    opt_failure("Loading of configuration failed");
}

void opt_conf_internal(struct opt_item * opt, const char * value, void * data UNUSED) {
  struct cf_context *cc = cf_get_context();
  switch (opt->letter) {
    case 'S':
      cf_load_default(cc);
      if (cf_set(value))
	opt_failure("Cannot set %s", value);
      break;
    case 'C':
      if (cf_load(value))
	opt_failure("Cannot load config file %s", value);
      break;
#ifdef CONFIG_UCW_DEBUG
    case '0':
      opt_conf_end_of_options(cc);
      struct fastbuf *b = bfdopen(1, 4096);
      cf_dump_sections(b);
      bclose(b);
      exit(0);
      break;
#endif
  }
}

void opt_conf_hook_internal(struct opt_item * opt, const char * value UNUSED, void * data UNUSED) {
  static enum {
    OPT_CONF_HOOK_BEGIN,
    OPT_CONF_HOOK_CONFIG,
    OPT_CONF_HOOK_OTHERS
  } state = OPT_CONF_HOOK_BEGIN;

  int confopt = 0;

  if (opt->letter == 'S' || opt->letter == 'C' || (opt->name && !strcmp(opt->name, "dumpconfig")))
    confopt = 1;

  switch (state) {
    case OPT_CONF_HOOK_BEGIN:
      if (confopt)
	state = OPT_CONF_HOOK_CONFIG;
      else {
	opt_conf_end_of_options(cf_get_context());
	state = OPT_CONF_HOOK_OTHERS;
      }
      break;
    case OPT_CONF_HOOK_CONFIG:
      if (!confopt) {
	opt_conf_end_of_options(cf_get_context());
	state = OPT_CONF_HOOK_OTHERS;
      }
      break;
    case OPT_CONF_HOOK_OTHERS:
      if (confopt)
	opt_failure("Config options (-C, -S) must stand before other options.");
      break;
    default:
      ASSERT(0);
  }
}
