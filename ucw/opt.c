/*
 *	UCW Library -- Parsing of command line options
 *
 *	(c) 2013 Jan Moskyto Matejka <mq@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/opt.h>
#include <ucw/conf.h>
#include <ucw/stkstring.h>
#include <ucw/strtonum.h>

#include <alloca.h>

static void opt_failure(const char * mesg, ...) FORMAT_CHECK(printf,1,2);
static void opt_failure(const char * mesg, ...) {
  va_list args;
  va_start(args, mesg);
  stk_vprintf(mesg, args);
  exit(OPT_EXIT_BAD_ARGS);
  va_end(args);
}

struct opt_section * opt_section_root;

void opt_help_noexit_internal(struct opt_section * help) {
  uns first_column = 0;

  for (struct opt_item * item = help->opt; item->cls != OPT_CL_END; item++) {
    if (item->flags & OPT_NO_HELP) continue;
    if (item->cls == OPT_CL_HELP && item->u.help2 == NULL) continue;
    if (item->cls == OPT_CL_SECTION) continue;
    
    uns linelen = 0;
    if (item->cls == OPT_CL_HELP) { // two-column help line
      if (first_column < strlen(item->help))
	first_column = strlen(item->help);
      continue;
    }
    
    if (item->letter) { // will write sth like "-x, --exclusive"
      linelen = strlen("-x, --") + strlen(item->name);
    }
    else { // will write sth like "--exclusive"
      linelen = strlen("--") + strlen(item->name);
    }

    ASSERT(item->flags & OPT_VALUE_FLAGS);

    if (item->flags & OPT_REQUIRED_VALUE) {
      linelen += strlen("=value");
    }
    else if (item->flags & OPT_MAYBE_VALUE) {
      linelen += strlen("(=value)");
    }

    if (linelen > first_column)
      first_column = linelen;
  }

  char * spaces = alloca(first_column + 1);
  char * buf = alloca(first_column + 1);
  for (uns i=0;i<first_column;i++)
    spaces[i] = ' ';

  spaces[first_column] = 0;

#define VAL(it) ((it->flags & OPT_REQUIRED_VALUE) ? "=value" : ((it->flags & OPT_NO_VALUE) ? "" : "(=value)"))
  for (struct opt_item * item = help->opt; item->cls != OPT_CL_END; item++) {
    if (item->flags & OPT_NO_HELP) continue;
    
    if (item->cls == OPT_CL_HELP) {
      fprintf(stderr, "%s", item->help);
      if (item->u.help2 == NULL)
	fprintf(stderr, "\n");
      else
	fprintf(stderr, "%s %s\n", spaces + strlen(item->help), item->u.help2);
    }
    else if (item->cls == OPT_CL_SECTION) {
      opt_help_noexit_internal(item->u.section);
    }
    else if (item->letter) {
      sprintf(buf, "-%c, --%s%s", item->letter, item->name, VAL(item));
      fprintf(stderr, "%s%s %s\n", buf, spaces + strlen(buf), item->help);
    }
    else {
      sprintf(buf, "--%s%s", item->name, VAL(item));
      fprintf(stderr, "%s%s %s\n", buf, spaces + strlen(buf), item->help);
    }
  }
}

struct opt_precomputed {
  struct opt_precomputed_option {
    struct opt_item * item;
    short flags;
    short count;
  } ** opts;
  struct opt_precomputed_option * shortopt[256];
  short opt_count;
};

static struct opt_item * opt_find_item_shortopt(int chr, struct opt_precomputed * opts) {
  return (opts->shortopt[chr] ? opts->shortopt[chr]->item : NULL);
}

static struct opt_item * opt_find_item_longopt(char * str, struct opt_precomputed * pre) {
  uns len = strlen(str);
  struct opt_item * candidate = NULL;

  for (int i=0; i<pre->opt_count; i++) {
    if (!strncmp(pre->opts[i]->item->name, str, len)) {
      if (strlen(pre->opts[i]->item->name) == len) {
	if (pre->opts[i]->count++ && (pre->opts[i]->flags & OPT_SINGLE))
	  opt_failure("Option %s appeared the second time.\n", pre->opts[i]->item->name);

	return pre->opts[i]->item;
      }
      if (candidate)
	opt_failure("Ambiguous prefix %s: Found matching %s and %s.\n", str, candidate->name, pre->opts[i]->item->name);
      else
	candidate = pre->opts[i]->item;
    }
  }

  if (candidate)
    return candidate;

  opt_failure("Invalid option %s.\n", str);
}

#define OPT_NAME (longopt ? stk_printf("--%s", item->name) : stk_printf("-%c", item->letter))
static void opt_parse_value(struct opt_item * item, char * value, int longopt) {
  switch (item->cls) {
    case OPT_CL_BOOL:
      if (!strcasecmp(value, "y") || !strcasecmp(value, "yes") || !strcasecmp(value, "true") || !strcasecmp(value, "1"))
	*((int *) item->ptr) = 1;
      else if (!strcasecmp(value, "n") || !strcasecmp(value, "no") || !strcasecmp(value, "false") || !strcasecmp(value, "0"))
	*((int *) item->ptr) = 0;
      else
	opt_failure("Boolean argument for %s has a strange value. Supported (case insensitive): y/n, yes/no, true/false.\n", OPT_NAME);
      break;
    case OPT_CL_STATIC:
      {
	char * e = NULL;
	switch (item->type) {
	  case CT_INT:
	    e = cf_parse_int(value, item->ptr);
	    if (e)
	      opt_failure("Integer value parsing failed for argument %s: %s\n", OPT_NAME, e);
	    break;
	  case CT_U64:
	    e = cf_parse_u64(value, item->ptr);
	    if (e)
	      opt_failure("Unsigned 64-bit value parsing failed for argument %s: %s\n", OPT_NAME, e);
	    break;
	  case CT_DOUBLE:
	    e = cf_parse_double(value, item->ptr);
	    if (e)
	      opt_failure("Double value parsing failed for argument %s: %s\n", OPT_NAME, e);
	    break;
	  case CT_IP:
	    e = cf_parse_ip(value, item->ptr);
	    if (e)
	      opt_failure("IP parsing failed for argument %s: %s\n", OPT_NAME, e);
	    break;
	  case CT_STRING:
	    item->ptr = strdup(value);
	    break;
	  default:
	    ASSERT(0);
	}
	break;
      }
    case OPT_CL_SWITCH:
      if (*((int *)item->ptr) != -1)
	opt_failure("Multiple switches: %s", OPT_NAME);
      else
	*((int *)item->ptr) = item->u.value;
      break;
    case OPT_CL_INC:
      if (item->flags & OPT_DECREMENT)
	(*((int *)item->ptr))--;
      else
	(*((int *)item->ptr))++;
    case OPT_CL_CALL:
      item->u.call(item, value, item->ptr);
      break;
    case OPT_CL_USER:
      {
	char * e = NULL;
	e = item->u.utype->parser(value, item->ptr);
	if (e)
	  opt_failure("User defined type value parsing failed for argument %s: %s\n", OPT_NAME, e);
	break;
      }
    default:
      ASSERT(0);
  }
}
#undef OPT_NAME

static int opt_longopt(char ** argv, int index, struct opt_precomputed * pre) {
  int eaten = 0;
  char * name_in = argv[index] + 2; // skipping the -- on the beginning
  uns pos = strchrnul(name_in, '=') - name_in;
  struct opt_item * item = opt_find_item_longopt(strndupa(name_in, pos), pre);
  char * value = NULL;
  if (item->flags & OPT_REQUIRED_VALUE) {
    if (pos < strlen(name_in))
      value = name_in + pos + 1;
    else {
      value = argv[index+1];
      eaten++;
    }
  }
  else if (item->flags & OPT_MAYBE_VALUE) {
    if (pos < strlen(name_in))
      value = name_in + pos + 1;
  }
  else {
    if (pos < strlen(name_in))
      opt_failure("Argument %s must not have any value.", item->name);
  }
  opt_parse_value(item, value, 1);
  return eaten;
}

static int opt_shortopt(char ** argv, int index, struct opt_precomputed * pre) {
  int chr = 0;
  struct opt_item * item;
  while (argv[index][++chr] && (item = opt_find_item_shortopt(argv[index][chr], pre))) {
    if (item->flags & OPT_NO_VALUE) {
      opt_parse_value(item, NULL, 0);
      continue;
    }
    if (chr == 1 && (item->flags & OPT_REQUIRED_VALUE)) {
      if (argv[index][2]) {
        opt_parse_value(item, argv[index] + 2, 0);
	return 0;
      }
      else {
	opt_parse_value(item, argv[index+1], 0);
	return 1;
      }
    }
    else if (chr == 1 && (item->flags & OPT_MAYBE_VALUE)) {
      if (argv[index][2])
        opt_parse_value(item, argv[index] + 2, 0);
      else
	opt_parse_value(item, NULL, 0);
    }
    else if (item->flags & (OPT_REQUIRED_VALUE | OPT_MAYBE_VALUE)) {
      if (argv[index][chr+1] || (item->flags | OPT_MAYBE_VALUE))
	opt_failure("Option -%c may or must have a value but found inside a bunch of short opts.", item->letter);
      else {
	opt_parse_value(item, argv[index+1], 0);
	return 1;
      }
    }
  }

  if (argv[index][chr])
    opt_failure("Unknown option -%c.", item->letter);
  
  return 0;
}

#define OPT_TRAVERSE_SECTIONS \
  do { \
    while (item->cls == OPT_CL_SECTION) { \
      if (stk->next) \
	stk = stk->next; \
      else { \
	struct opt_stack * new_stk = alloca(sizeof(*new_stk)); \
	new_stk->prev = stk; \
	stk->next = new_stk; \
	stk = new_stk; \
      } \
      stk->this = item; \
      item = item->u.section->opt; \
    } \
    if (item->cls == OPT_CL_END) { \
      if (!stk) break; \
      item = stk->this; \
      stk = stk->prev; \
      continue; \
    } \
  } while (0)

void opt_parse(const struct opt_section * options, char ** argv, opt_positional * callback) {
  struct opt_stack {
    struct opt_item * this;
    struct opt_stack * prev;
    struct opt_stack * next;
  } * stk = alloca(sizeof(*stk));
  stk->this = NULL;
  stk->prev = NULL;
  stk->next = NULL;

  struct opt_precomputed * pre = alloca(sizeof(*pre));

  int count;

  for (struct opt_item * item = options->opt; ; item++) {
    OPT_TRAVERSE_SECTIONS;
    if (item->letter || item->name)
      count++;
  }
  
  pre->opts = xmalloc(sizeof(*pre->opts) * count);
  pre->opt_count = 0;

  for (struct opt_item * item = options->opt; ; item++) {
    OPT_TRAVERSE_SECTIONS;
    if (item->letter || item->name) {
      struct opt_precomputed_option * opt = xmalloc(sizeof(*opt));
      opt->item = item;
      opt->flags = item->flags;
      opt->count = 0;
      pre->opts[pre->opt_count++] = opt;
      if (item->letter)
	pre->shortopt[(int) item->letter] = opt;
      if (!(opt->flags & OPT_VALUE_FLAGS) &&
	  (item->cls == OPT_CL_CALL || item->cls == OPT_CL_USER)) {
	fprintf(stderr, "You MUST specify some of the value flags for the %c/%s item.\n", item->letter, item->name);
	ASSERT(0);
      }
      else
	opt->flags |= opt_default_value_flags[item->cls];
    }
  }

  int force_positional = 0;
  for (int i=0;argv[i];i++) {
    if (argv[i][0] != '-' || force_positional) {
      callback(argv[i]);
    }
    else {
      if (argv[i][1] == '-') {
	if (argv[i][2] == '\0')
	  force_positional++;
	else
	  i += opt_longopt(argv, i, pre);
      }
      else if (argv[i][1])
	i += opt_shortopt(argv, i, pre);
      else
	callback(argv[i]);
    }
  }
}

#ifdef TEST
#include <ucw/fastbuf.h>

static void show_version(struct opt_item * opt UNUSED, const char * value UNUSED, void * data UNUSED) {
  printf("This is a simple tea boiling console v0.1.\n");
  exit(EXIT_SUCCESS);
}

struct teapot_temperature {
  enum {
    TEMP_CELSIUS = 0,
    TEMP_FAHRENHEIT,
    TEMP_KELVIN,
    TEMP_REAUMUR,
    TEMP_RANKINE
  } scale;
  int value;
} temperature;

static char * temp_scale_str[] = { "C", "F", "K", "Re", "R" };

static enum TEAPOT_TYPE {
  TEAPOT_STANDARD = 0,
  TEAPOT_EXCLUSIVE,
  TEAPOT_GLASS,
  TEAPOT_HANDS,
  TEAPOT_UNDEFINED = -1
} set = TEAPOT_UNDEFINED;

static int english = 0;
static char * name = NULL;
static int sugar = 0;
static int verbose = 1;
static int with_gas = 0;
static int black_magic = 0;
static int pray = 0;
static int water_amount = 0;

static const char * teapot_temperature_parser(char * in, void * ptr) {
  struct teapot_temperature * temp = ptr;
  const char * next;
  const char * err = str_to_int(&temp->value, in, &next, 0);
  if (err)
    return err;
  if (!strcmp("C", next))
    temp->scale = TEMP_CELSIUS;
  else if (!strcmp("F", next))
    temp->scale = TEMP_FAHRENHEIT;
  else if (!strcmp("K", next))
    temp->scale = TEMP_KELVIN;
  else if (!strcmp("R", next))
    temp->scale = TEMP_RANKINE;
  else if (!strcmp("Re", next))
    temp->scale = TEMP_REAUMUR;
  else {
    fprintf(stderr, "Unknown scale: %s\n", next);
    exit(OPT_EXIT_BAD_ARGS);
  }
  return next + strlen(next);
}

static void teapot_temperature_dumper(struct fastbuf * fb, void * ptr) {
  struct teapot_temperature * temp = ptr;
  bprintf(fb, "%d%s", temp->value, temp_scale_str[temp->scale]);
}

static struct cf_user_type teapot_temperature_t = {
  .size = sizeof(struct teapot_temperature),
  .name = "teapot_temperature_t",
  .parser = (cf_parser1*) teapot_temperature_parser,
  .dumper = (cf_dumper1*) teapot_temperature_dumper
};

static struct opt_section water_options = {
  OPT_ITEMS {
    OPT_INT('w', "water", water_amount, OPT_REQUIRED | OPT_REQUIRED_VALUE, "Amount of water (in mls)"),
    OPT_BOOL('G', "with-gas", with_gas, OPT_NO_VALUE, "Use water with gas"),
    OPT_END
  }
};

static struct opt_section help = {
  OPT_ITEMS {
    OPT_HELP("A simple tea boiling console."),
    OPT_HELP("Usage: teapot [options] name-of-the-tea"),
    OPT_HELP("Black, green or white tea supported as well as fruit or herbal tea."),
    OPT_HELP("You may specify more kinds of tea, all of them will be boiled for you, in the given order."),
    OPT_HELP(""),
    OPT_HELP("Options:"),
    OPT_HELP_OPTION,
    OPT_CALL('V', "version", show_version, NULL, OPT_NO_VALUE, "Show the version"),
    OPT_HELP(""),
    OPT_BOOL('e', "english-style", english, 0, "English style (with milk)"),
    OPT_INT('s', "sugar", sugar, OPT_REQUIRED_VALUE, "Amount of sugar (in teaspoons)"),
    OPT_SWITCH(0, "standard-set", set, TEAPOT_STANDARD, 0, "Standard teapot"),
    OPT_SWITCH('x', "exclusive-set", set, TEAPOT_EXCLUSIVE, 0, "Exclusive teapot"),
    OPT_SWITCH('g', "glass-set", set, TEAPOT_GLASS, 0, "Transparent glass teapot"),
    OPT_SWITCH('h', "hands", set, TEAPOT_HANDS, 0, "Use user's hands as a teapot (a bit dangerous)"),
    OPT_USER('t', "temperature", temperature, teapot_temperature_t, OPT_REQUIRED_VALUE,
		  "Wanted final temperature of the tea to be served\n"
	      "\t\tSupported scales:\tCelsius [60C], Fahrenheit [140F],"
	      "\t\t\tKelvin [350K], Rankine [600R] and Reaumur [50Re]"
	      "\t\tOnly integer values allowed."),
    OPT_INC('v', "verbose", verbose, 0, "Verbose (the more -v, the more verbose)"),
    OPT_INC('q', "quiet", verbose, OPT_DECREMENT, "Quiet (the more -q, the more quiet)"),
    OPT_INT('b', "black-magic", black_magic, 0, "Use black magic to make the tea extraordinary delicious"),
    OPT_BOOL('p', "pray", pray, 0, "Pray before boiling"),
    OPT_HELP(""),
    OPT_HELP("Water options:"),
    OPT_SECTION(water_options),
    OPT_END
  }
};

#define MAX_TEA_COUNT 30
static char * tea_list[MAX_TEA_COUNT];
static int tea_num = 0;
static void add_tea(const char * name) {
  if (tea_num >= MAX_TEA_COUNT) {
    fprintf(stderr, "Cannot boil more than %d teas.\n", MAX_TEA_COUNT);
    exit(OPT_EXIT_BAD_ARGS);
  }
  tea_list[tea_num++] = strdup(name);
}

static void boil_tea(const char * name) {
  printf("Boiling a tea: %s\n", name);
}

int main(int argc, char ** argv)
{
  opt_parse(&help, argv, add_tea);

  for (int i=0; i<tea_num; i++)
    boil_tea(tea_list[i]);

  printf("Everything OK. Bye.\n");
}

#endif
