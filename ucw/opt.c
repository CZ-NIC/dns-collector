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
#include <math.h>

static void opt_failure(const char * mesg, ...) FORMAT_CHECK(printf,1,2) NONRET;
static void opt_failure(const char * mesg, ...) {
  va_list args;
  va_start(args, mesg);
  vfprintf(stderr, mesg, args);
  fprintf(stderr, "\n");
  opt_usage();
  exit(OPT_EXIT_BAD_ARGS);
  va_end(args);
}

#define OPT_ADD_DEFAULT_ITEM_FLAGS(item, flags) \
  do { \
    if (item->letter >= 256) { \
      if (flags & OPT_VALUE_FLAGS) \
	flags &= ~OPT_VALUE_FLAGS; \
      flags |= OPT_REQUIRED_VALUE; \
    } \
    if (!(flags & OPT_VALUE_FLAGS) && \
	(item->cls == OPT_CL_CALL || item->cls == OPT_CL_USER)) { \
      fprintf(stderr, "You MUST specify some of the value flags for the %c/%s item.\n", item->letter, item->name); \
      ASSERT(0); \
    } \
    else if (!(flags & OPT_VALUE_FLAGS)) \
      flags |= opt_default_value_flags[item->cls]; \
  } while (0)
#define OPT_ITEM_FLAGS(item) ((item->flags & OPT_VALUE_FLAGS) ? item->flags : item->flags | opt_default_value_flags[item->cls])

const struct opt_section * opt_section_root;

#define FOREACHLINE(text) for (const char * begin = (text), * end = (text); (*end) && (end = strchrnul(begin, '\n')); begin = end+1)

void opt_help_internal(const struct opt_section * help) {
  int sections_cnt = 0;
  int lines_cnt = 0;

  for (struct opt_item * item = help->opt; item->cls != OPT_CL_END; item++) {
    if (item->flags & OPT_NO_HELP) continue;
    if (item->cls == OPT_CL_SECTION) {
      sections_cnt++;
      continue;
    }
    if (!*(item->help)) {
      lines_cnt++;
      continue;
    }
    FOREACHLINE(item->help)
      lines_cnt++;
  }

  struct opt_sectlist {
    int pos;
    struct opt_section * sect;
  } sections[sections_cnt];
  int s = 0;

  const char *lines[lines_cnt][3];
  memset(lines, 0, sizeof(lines));
  int line = 0;

  int linelengths[3] = { -1, -1, -1 };

  for (struct opt_item * item = help->opt; item->cls != OPT_CL_END; item++) {
    if (item->flags & OPT_NO_HELP) continue;

    if (item->cls == OPT_CL_HELP) {
      if (!*(item->help)) {
	line++;
	continue;
      }
#define SPLITLINES(text) do { \
      FOREACHLINE(text) { \
	int cell = 0; \
	for (const char * b = begin, * e = begin; (e < end) && (e = strchrnul(b, '\t')) && (e > end ? (e = end) : end); b = e+1) { \
	  lines[line][cell] = b; \
	  if (cell >= 2) \
	    break; \
	  else \
	    if (*e == '\t' && linelengths[cell] < (e - b)) \
	      linelengths[cell] = e-b; \
	  cell++; \
	} \
	line++; \
      } } while (0)
      SPLITLINES(item->help);
      continue;
    }

    if (item->cls == OPT_CL_SECTION) {
      sections[s++] = (struct opt_sectlist) { .pos = line, .sect = item->u.section };
      continue;
    }

    uns valoff = strchrnul(item->help, '\t') - item->help;
    uns eol = strchrnul(item->help, '\n') - item->help;
    if (valoff > eol)
      valoff = eol;
#define VAL(it) ((OPT_ITEM_FLAGS(it) & OPT_REQUIRED_VALUE) ? stk_printf("=%.*s", valoff, item->help)  : ((OPT_ITEM_FLAGS(it) & OPT_NO_VALUE) ? "" : stk_printf("(=%.*s)", valoff, item->help)))
    if (item->name) {
      lines[line][1] = stk_printf("--%s%s", item->name, VAL(item));
      if (linelengths[1] < (int) strlen(lines[line][1]))
	linelengths[1] = strlen(lines[line][1]);
      lines[line][0] = "";
      if (linelengths[0] < 0)
	linelengths[0] = 0;
    }
    if (item->letter) {
      lines[line][0] = stk_printf("-%c,", item->letter);
      if (linelengths[0] < (int) strlen(lines[line][0]))
	linelengths[0] = strlen(lines[line][0]);
    }
#undef VAL

    if (eol > valoff) {
      lines[line][2] = item->help + valoff + 1;
    }

    line++;

    if (*(item->help + eol))
      SPLITLINES(item->help + eol + 1);
  }
#undef SPLITLINES

  s = 0;
#define FIELD(k) linelengths[k], MIN(strchrnul(lines[i][k], '\t')-lines[i][k],strchrnul(lines[i][k], '\n')-lines[i][k]), lines[i][k]
#define LASTFIELD(k) MIN(strchrnul(lines[i][k], '\t')-lines[i][k],strchrnul(lines[i][k], '\n')-lines[i][k]), lines[i][k]
  for (int i=0;i<line;i++) {
    while (s < sections_cnt && sections[s].pos == i) {
      opt_help_internal(sections[s].sect);
      s++;
    }
    if (lines[i][0] == NULL)
      printf("\n");
    else if (linelengths[0] == -1 || lines[i][1] == NULL)
      printf("%.*s\n", LASTFIELD(0));
    else if (linelengths[1] == -1 || lines[i][2] == NULL)
      printf("%-*.*s  %.*s\n", FIELD(0), LASTFIELD(1));
    else
      printf("%-*.*s  %-*.*s  %.*s\n", FIELD(0), FIELD(1), LASTFIELD(2));
  }
  while (s < sections_cnt && sections[s].pos == line) {
    opt_help_internal(sections[s].sect);
    s++;
  }
}

static int opt_positional_max = 0;
static int opt_positional_count = 0;

struct opt_precomputed {
  struct opt_precomputed_option {
    struct opt_item * item;
    const char * name;
    short flags;
    short count;
  } ** opts;
  struct opt_precomputed_option ** shortopt;
  short opt_count;
};

static struct opt_precomputed_option * opt_find_item_shortopt(int chr, struct opt_precomputed * pre) {
  struct opt_precomputed_option * candidate = pre->shortopt[chr];
  if (!candidate)
    opt_failure("Invalid option -%c", chr);
  if (candidate->count++ && (candidate->flags & OPT_SINGLE))
    opt_failure("Option -%c appeared the second time.", candidate->item->letter);
  return candidate;
}

static struct opt_precomputed_option * opt_find_item_longopt(char * str, struct opt_precomputed * pre) {
  uns len = strlen(str);
  struct opt_precomputed_option * candidate = NULL;

  for (int i=0; i<pre->opt_count; i++) {
    if (!strncmp(pre->opts[i]->name, str, len)) {
      if (strlen(pre->opts[i]->name) == len) {
	if (pre->opts[i]->count++ && (pre->opts[i]->flags & OPT_SINGLE))
	  opt_failure("Option %s appeared the second time.", pre->opts[i]->name);

	return pre->opts[i];
      }
      if (candidate)
	opt_failure("Ambiguous prefix %s: Found matching %s and %s.", str, candidate->name, pre->opts[i]->name);
      else
	candidate = pre->opts[i];
    }
    if (!strncmp("no-", str, 3) && !strncmp(pre->opts[i]->name, str+3, len-3)) {
      if (strlen(pre->opts[i]->name) == len-3) {
	if (pre->opts[i]->count++ && (pre->opts[i]->flags & OPT_SINGLE))
	  opt_failure("Option %s appeared the second time.", pre->opts[i]->name);

	return pre->opts[i];
      }
      if (candidate)
	opt_failure("Ambiguous prefix %s: Found matching %s and %s.", str, candidate->name, pre->opts[i]->name);
      else
	candidate = pre->opts[i];
    }
  }

  if (candidate)
    return candidate;

  opt_failure("Invalid option %s.", str);
}

#define OPT_NAME (longopt == 2 ? stk_printf("positional arg #%d", opt_positional_count) : (longopt == 1 ? stk_printf("--%s", opt->name) : stk_printf("-%c", item->letter)))
static void opt_parse_value(struct opt_precomputed_option * opt, char * value, int longopt) {
  struct opt_item * item = opt->item;
  switch (item->cls) {
    case OPT_CL_BOOL:
      if (!value || !strcasecmp(value, "y") || !strcasecmp(value, "yes") || !strcasecmp(value, "true") || !strcasecmp(value, "1"))
	*((int *) item->ptr) = 1 ^ (!!(opt->flags & OPT_NEGATIVE));
      else if (!strcasecmp(value, "n") || !strcasecmp(value, "no") || !strcasecmp(value, "false") || !strcasecmp(value, "0"))
	*((int *) item->ptr) = 0 ^ (!!(opt->flags & OPT_NEGATIVE));
      else
	opt_failure("Boolean argument for %s has a strange value. Supported (case insensitive): y/n, yes/no, true/false.", OPT_NAME);
      break;
    case OPT_CL_STATIC:
      {
	char * e = NULL;
	switch (item->type) {
	  case CT_INT:
	    if (!value)
	      *((int*)item->ptr) = 0;
	    else
	      e = cf_parse_int(value, item->ptr);
	    if (e)
	      opt_failure("Integer value parsing failed for %s: %s", OPT_NAME, e);
	    break;
	  case CT_U64:
	    if (!value)
	      *((u64*)item->ptr) = 0;
	    else
	      e = cf_parse_u64(value, item->ptr);
	    if (e)
	      opt_failure("Unsigned 64-bit value parsing failed for %s: %s", OPT_NAME, e);
	    break;
	  case CT_DOUBLE:
	    if (!value)
	      *((double*)item->ptr) = NAN;
	    else
	      e = cf_parse_double(value, item->ptr);
	    if (e)
	      opt_failure("Double value parsing failed for %s: %s", OPT_NAME, e);
	    break;
	  case CT_IP:
	    if (!value)
	      e = cf_parse_ip("0.0.0.0", item->ptr);
	    else
	      e = cf_parse_ip(value, item->ptr);
	    if (e)
	      opt_failure("IP parsing failed for %s: %s", OPT_NAME, e);
	    break;
	  case CT_STRING:
	    if (!value)
	      item->ptr = NULL;
	    else
	      *((const char **) (item->ptr)) = xstrdup(value);
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
	e = item->u.utype->parser(value, item->ptr);
	if (e)
	  opt_failure("User defined type value parsing failed for %s: %s", OPT_NAME, e);
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
  struct opt_precomputed_option * opt = opt_find_item_longopt(strndupa(name_in, pos), pre);
  char * value = NULL;

  if (opt->item->cls == OPT_CL_BOOL && !strncmp(name_in, "no-", 3) && !strncmp(name_in+3, opt->item->name, pos-3))
    value = "n";
  else if (opt->flags & OPT_REQUIRED_VALUE) {
    if (pos < strlen(name_in))
      value = name_in + pos + 1;
    else {
      value = argv[index+1];
      if (!value)
	opt_failure("Argument --%s must have a value but nothing supplied.", opt->name);
      eaten++;
    }
  }
  else if (opt->flags & OPT_MAYBE_VALUE) {
    if (pos < strlen(name_in))
      value = name_in + pos + 1;
  }
  else {
    if (pos < strlen(name_in))
      opt_failure("Argument --%s must not have any value.", opt->name);
  }
  opt_parse_value(opt, value, 1);
  return eaten;
}

static int opt_shortopt(char ** argv, int index, struct opt_precomputed * pre) {
  int chr = 0;
  struct opt_precomputed_option * opt;
  while (argv[index][++chr] && (opt = opt_find_item_shortopt(argv[index][chr], pre))) {
    if (opt->flags & OPT_NO_VALUE) {
      opt_parse_value(opt, NULL, 0);
    }
    else if (opt->flags & OPT_REQUIRED_VALUE) {
      if (chr == 1 && argv[index][2]) {
        opt_parse_value(opt, argv[index] + 2, 0);
	return 0;
      }
      else if (argv[index][chr+1])
	opt_failure("Option -%c must have a value but found inside a bunch of short opts.", opt->item->letter);
      else if (!argv[index+1])
	opt_failure("Option -%c must have a value but nothing supplied.", opt->item->letter);
      else {
	opt_parse_value(opt, argv[index+1], 0);
	return 1;
      }
    }
    else if (opt->flags & OPT_MAYBE_VALUE) {
      if (chr == 1 && argv[index][2]) {
        opt_parse_value(opt, argv[index] + 2, 0);
	return 0;
      }
      else
	opt_parse_value(opt, NULL, 0);
    }
    else {
      ASSERT(0);
    }
  }

  if (argv[index][chr])
    opt_failure("Unknown option -%c.", argv[index][chr]);
  
  return 0;
}

static void opt_positional(char * value, struct opt_precomputed * pre) {
  opt_positional_count++;
  struct opt_precomputed_option * opt = opt_find_item_shortopt((opt_positional_count > opt_positional_max ? 256 : opt_positional_count + 256), pre);
  if (!opt) {
    ASSERT(opt_positional_count > opt_positional_max);
    opt_failure("Too many positional args.");
  }

  opt_parse_value(opt, value, 2);
}

#define OPT_TRAVERSE_SECTIONS \
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
      if (!stk->prev) break; \
      item = stk->this; \
      stk = stk->prev; \
      continue; \
    }

void opt_parse(const struct opt_section * options, char ** argv) {
  opt_section_root = options;

  struct opt_stack {
    struct opt_item * this;
    struct opt_stack * prev;
    struct opt_stack * next;
  } * stk = alloca(sizeof(*stk));
  stk->this = NULL;
  stk->prev = NULL;
  stk->next = NULL;

  struct opt_precomputed * pre = alloca(sizeof(*pre));
  memset(pre, 0, sizeof (*pre));

  int count = 0;

  for (struct opt_item * item = options->opt; ; item++) {
    OPT_TRAVERSE_SECTIONS;
    if (item->letter || item->name)
      count++;
    if (item->cls == OPT_CL_BOOL)
      count++;
    if (item->letter > 256)
      opt_positional_max++;
  }
  
  pre->opts = alloca(sizeof(*pre->opts) * count);
  pre->shortopt = alloca(sizeof(*pre->shortopt) * (opt_positional_max + 257));
  memset(pre->shortopt, 0, sizeof(*pre->shortopt) * (opt_positional_max + 257));
  
  pre->opt_count = 0;

  for (struct opt_item * item = options->opt; ; item++) {
    OPT_TRAVERSE_SECTIONS;
    if (item->letter || item->name) {
      struct opt_precomputed_option * opt = xmalloc(sizeof(*opt));
      opt->item = item;
      opt->flags = item->flags;
      opt->count = 0;
      opt->name = item->name;
      pre->opts[pre->opt_count++] = opt;
      if (item->letter)
	pre->shortopt[(int) item->letter] = opt;
      OPT_ADD_DEFAULT_ITEM_FLAGS(item, opt->flags);
    }
  }

  int force_positional = 0;
  for (int i=0;argv[i];i++) {
    if (argv[i][0] != '-' || force_positional) {
      opt_positional(argv[i], pre);
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
	opt_positional(argv[i], pre);
    }
  }

  for (int i=0;i<opt_positional_max+257;i++) {
    if (!pre->shortopt[i])
      continue;
    if (!pre->shortopt[i]->count && (pre->shortopt[i]->flags & OPT_REQUIRED))
      if (i < 256)
        opt_failure("Required option -%c not found.\n", pre->shortopt[i]->item->letter);
      else
	opt_failure("Required positional argument #%d not found.\n", (i > 256) ? pre->shortopt[i]->item->letter-256 : opt_positional_max+1);
  }

  for (int i=0;i<pre->opt_count;i++) {
    if (!pre->opts[i])
      continue;
    if (!pre->opts[i]->count && (pre->opts[i]->flags & OPT_REQUIRED))
      opt_failure("Required option --%s not found.\n", pre->opts[i]->item->name);
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

static char * teapot_type_str[] = { "standard", "exclusive", "glass", "hands" };

static int english = 0;
static int sugar = 0;
static int verbose = 1;
static int with_gas = 0;
static int black_magic = 0;
static int pray = 0;
static int water_amount = 0;
static char * first_tea = NULL;

#define MAX_TEA_COUNT 30
static char * tea_list[MAX_TEA_COUNT];
static int tea_num = 0;
static void add_tea(struct opt_item * opt UNUSED, const char * name, void * data) {
  char ** tea_list = data;
  if (tea_num >= MAX_TEA_COUNT) {
    fprintf(stderr, "Cannot boil more than %d teas.\n", MAX_TEA_COUNT);
    exit(OPT_EXIT_BAD_ARGS);
  }
  tea_list[tea_num++] = xstrdup(name);
}

static const char * teapot_temperature_parser(char * in, void * ptr) {
  struct teapot_temperature * temp = ptr;
  const char * next;
  const char * err = str_to_int(&temp->value, in, &next, 10);
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
  return NULL;
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
    OPT_INT('w', "water", water_amount, OPT_REQUIRED | OPT_REQUIRED_VALUE, "<volume>\tAmount of water (in mls; required)"),
    OPT_BOOL('G', "with-gas", with_gas, OPT_NO_VALUE, "\tUse water with gas"),
    OPT_END
  }
};

static struct opt_section help = {
  OPT_ITEMS {
    OPT_HELP("A simple tea boiling console."),
    OPT_HELP("Usage: teapot [options] name-of-the-tea"),
    OPT_HELP("Black, green or white tea supported as well as fruit or herbal tea."),
    OPT_HELP("You may specify more kinds of tea, all of them will be boiled for you, in the given order."),
    OPT_HELP("At least one kind of tea must be specified."),
    OPT_HELP(""),
    OPT_HELP("Options:"),
    OPT_HELP_OPTION,
    OPT_CALL('V', "version", show_version, NULL, OPT_NO_VALUE, "\tShow the version"),
    OPT_HELP(""),
    OPT_BOOL('e', "english-style", english, 0, "\tEnglish style (with milk)"),
    OPT_INT('s', "sugar", sugar, OPT_REQUIRED_VALUE, "<spoons>\tAmount of sugar (in teaspoons)"),
    OPT_SWITCH(0, "standard-set", set, TEAPOT_STANDARD, 0, "\tStandard teapot"),
    OPT_SWITCH('x', "exclusive-set", set, TEAPOT_EXCLUSIVE, 0, "\tExclusive teapot"),
    OPT_SWITCH('g', "glass-set", set, TEAPOT_GLASS, 0, "\tTransparent glass teapot"),
    OPT_SWITCH('h', "hands", set, TEAPOT_HANDS, 0, "\tUse user's hands as a teapot (a bit dangerous)"),
    OPT_USER('t', "temperature", temperature, teapot_temperature_t, OPT_REQUIRED_VALUE | OPT_REQUIRED,
		  "<value>\tWanted final temperature of the tea to be served (required)\n"
	      "\t\tSupported scales:  Celsius [60C], Fahrenheit [140F],\n"
	      "\t\t                   Kelvin [350K], Rankine [600R] and Reaumur [50Re]\n"
	      "\t\tOnly integer values allowed."),
    OPT_INC('v', "verbose", verbose, 0, "\tVerbose (the more -v, the more verbose)"),
    OPT_INC('q', "quiet", verbose, OPT_NEGATIVE, "\tQuiet (the more -q, the more quiet)"),
    OPT_INT('b', "black-magic", black_magic, 0, "<strength>\tUse black magic to make the tea extraordinary delicious"),
    OPT_BOOL('p', "pray", pray, OPT_SINGLE, "\tPray before boiling"),
    OPT_STRING(OPT_POSITIONAL(1), NULL, first_tea, OPT_REQUIRED | OPT_NO_HELP, ""),
    OPT_CALL(OPT_POSITIONAL_TAIL, NULL, add_tea, &tea_list, OPT_NO_HELP, ""),
    OPT_HELP(""),
    OPT_HELP("Water options:"),
    OPT_SECTION(water_options),
    OPT_END
  }
};

int main(int argc UNUSED, char ** argv)
{
  opt_parse(&help, argv+1);

  printf("English style: %s|", english ? "yes" : "no");
  if (sugar)
    printf("Sugar: %d teaspoons|", sugar);
  if (set != -1)
    printf("Chosen teapot: %s|", teapot_type_str[set]);
  printf("Temperature: %d%s|", temperature.value, temp_scale_str[temperature.scale]);
  printf("Verbosity: %d|", verbose);
  if (black_magic)
    printf("Black magic: %d|", black_magic);
  printf("Prayer: %s|", pray ? "yes" : "no");
  printf("Water amount: %d|", water_amount);
  printf("Gas: %s|", with_gas ? "yes" : "no");
  printf("First tea: %s|", first_tea);
  for (int i=0; i<tea_num; i++)
    printf("Boiling a tea: %s|", tea_list[i]);

  printf("Everything OK. Bye.\n");
}

#endif
