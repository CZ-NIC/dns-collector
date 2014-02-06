/*
 *	UCW Library -- Parsing of command-line options
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
#include <ucw/gary.h>
#include <ucw/stkstring.h>
#include <ucw/strtonum.h>

#include <alloca.h>
#include <math.h>

static uns opt_default_value_flags[] = {
    [OPT_CL_BOOL] = OPT_NO_VALUE,
    [OPT_CL_STATIC] = OPT_MAYBE_VALUE,
    [OPT_CL_MULTIPLE] = OPT_REQUIRED_VALUE,
    [OPT_CL_SWITCH] = OPT_NO_VALUE,
    [OPT_CL_INC] = OPT_NO_VALUE,
    [OPT_CL_CALL] = 0,
    [OPT_CL_SECTION] = 0,
    [OPT_CL_HELP] = 0
};

void opt_failure(const char * mesg, ...) {
  va_list args;
  va_start(args, mesg);
  vfprintf(stderr, mesg, args);
  fprintf(stderr, "\nRun with --help for more information.\n");
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

void opt_precompute(struct opt_precomputed *opt, struct opt_item *item)
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
    ASSERT(item->cls != OPT_CL_CALL);
    flags |= opt_default_value_flags[item->cls];
  }

  opt->flags = flags;
}

static void opt_invoke_hooks(struct opt_context *oc, uns event, struct opt_item *item, char *value)
{
  for (int i = 0; i < oc->hook_count; i++) {
    struct opt_item *hook = oc->hooks[i];
    if (hook->flags & event) {
      void *data = (hook->flags & OPT_HOOK_INTERNAL) ? oc : hook->ptr;
      hook->u.hook(item, event, value, data);
    }
  }
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

static void opt_parse_value(struct opt_context * oc, struct opt_precomputed * opt, char * value) {
  struct opt_item * item = opt->item;

  if (opt->count++ && (opt->flags & OPT_SINGLE))
    opt_failure("Option %s must be specified at most once.", THIS_OPT);

  if (opt->flags & OPT_LAST_ARG)
    oc->stop_parsing = 1;

  opt_invoke_hooks(oc, OPT_HOOK_BEFORE_VALUE, item, value);

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
    case OPT_CL_MULTIPLE:
      {
	char * e = NULL;
	void * ptr;
	if (item->cls == OPT_CL_STATIC)
	  ptr = item->ptr;
	else
	  ptr = GARY_PUSH_GENERIC(*(void **)item->ptr);
#define OPT_PTR(type) ((type *) ptr)
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
	  case CT_USER:
	      {
		char * e = item->u.utype->parser(value, ptr);
		if (e)
		  opt_failure("Cannot parse the value of %s: %s", THIS_OPT, e);
		break;
	      }
	  default:
	    ASSERT(0);
	}
#undef OPT_PTR
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
      {
	void *data = (opt->flags & OPT_INTERNAL) ? oc : item->ptr;
	item->u.call(item, value, data);
	break;
      }
    default:
      ASSERT(0);
  }

  opt_invoke_hooks(oc, OPT_HOOK_AFTER_VALUE, item, value);
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
    else if (item->cls == OPT_CL_HOOK)
      oc->hook_count++;
    else if (item->letter || item->name) {
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
    else if (item->cls == OPT_CL_HOOK)
      oc->hooks[oc->hook_count++] = item;
    else if (item->letter || item->name) {
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

int opt_parse(const struct opt_section * options, char ** argv) {
  struct opt_context * oc = alloca(sizeof(*oc));
  memset(oc, 0, sizeof (*oc));
  oc->options = options;

  opt_count_items(oc, options);
  oc->opts = alloca(sizeof(*oc->opts) * oc->opt_count);
  oc->shortopt = alloca(sizeof(*oc->shortopt) * (oc->positional_max + OPT_POSITIONAL_TAIL + 1));
  memset(oc->shortopt, 0, sizeof(*oc->shortopt) * (oc->positional_max + OPT_POSITIONAL_TAIL + 1));
  oc->hooks = alloca(sizeof (*oc->hooks) * oc->hook_count);

  oc->opt_count = 0;
  oc->hook_count = 0;
  opt_prepare_items(oc, options);

  int force_positional = 0;
  int i;
  for (i=0; argv[i] && !oc->stop_parsing; i++) {
    char *arg = argv[i];
    opt_invoke_hooks(oc, OPT_HOOK_BEFORE_ARG, NULL, NULL);
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
  opt_invoke_hooks(oc, OPT_HOOK_FINAL, NULL, NULL);
  return i;
}
