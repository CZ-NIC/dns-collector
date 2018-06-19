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
#include <ucw/strtonum.h>
#include <ucw/fastbuf.h>
#include <ucw/gary.h>

static void show_version(const struct opt_item * opt UNUSED, const char * value UNUSED, void * data UNUSED) {
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

static int show_hooks = 0;
static int english = 0;
static int sugar = 0;
static int verbose = 1;
static int with_gas = 0;
static int *black_magic;
static int pray = 0;
static int water_amount = 0;
static int clean_pot = 1;
static char * first_tea = NULL;

#define MAX_TEA_COUNT 30
static char * tea_list[MAX_TEA_COUNT];
static int tea_num = 0;
static void add_tea(const struct opt_item * opt UNUSED, const char * name, void * data) {
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

static void opt_test_hook(const struct opt_item * opt, uint event UNUSED, const char * value, void * data) {
  if (!show_hooks)
    return;
  if (opt)
    printf("[HOOK-%s:%c/%s=%s] ", (char *) data, opt->letter, opt->name, value);
  else
    printf("[HOOK-%s] ", (char *) data);
}

static struct opt_section water_options = {
  OPT_ITEMS {
    OPT_INT('w', "water", water_amount, OPT_REQUIRED | OPT_REQUIRED_VALUE, "<volume>\tAmount of water (in mls; required)"),
    OPT_BOOL('G', "with-gas", with_gas, OPT_NO_VALUE, "\tUse water with gas"),
    OPT_END
  }
};

static struct opt_section options = {
  OPT_ITEMS {
    OPT_HELP("A simple tea boiling console."),
    OPT_HELP("Usage: teapot [options] name-of-the-tea"),
    OPT_HELP(""),
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
    OPT_SWITCH(0, "standard-set", set, TEAPOT_STANDARD, OPT_SINGLE, "\tStandard teapot"),
    OPT_SWITCH('x', "exclusive-set", set, TEAPOT_EXCLUSIVE, OPT_SINGLE, "\tExclusive teapot"),
    OPT_SWITCH('g', "glass-set", set, TEAPOT_GLASS, OPT_SINGLE, "\tTransparent glass teapot"),
    OPT_SWITCH('h', "hands", set, TEAPOT_HANDS, OPT_SINGLE, "\tUse user's hands as a teapot (a bit dangerous)"),
    OPT_USER('t', "temperature", temperature, teapot_temperature_t, OPT_REQUIRED_VALUE | OPT_REQUIRED,
		  "<value>\tWanted final temperature of the tea to be served (required)\n"
	      "\t\tSupported scales:  Celsius [60C], Fahrenheit [140F],\n"
	      "\t\t                   Kelvin [350K], Rankine [600R] and Reaumur [50Re]\n"
	      "\t\tOnly integer values allowed."),
    OPT_INC('v', "verbose", verbose, 0, "\tVerbose (the more -v, the more verbose)"),
    OPT_INC('q', "quiet", verbose, OPT_NEGATIVE, "\tQuiet (the more -q, the more quiet)"),
    OPT_INT_MULTIPLE('b', NULL, black_magic, 0, "<strength>\tUse black magic to make the tea extraordinarily delicious.\n\t\tMay be specified more than once to describe the amounts of black magic to be invoked in each step of tea boiling."),
    OPT_BOOL('p', "pray", pray, OPT_SINGLE, "\tPray before boiling"),
    OPT_BOOL(0, "no-clean", clean_pot, OPT_NEGATIVE, "\tDo not clean the teapot before boiling"),
    OPT_STRING(OPT_POSITIONAL(1), NULL, first_tea, OPT_REQUIRED, ""),
    OPT_CALL(OPT_POSITIONAL_TAIL, NULL, add_tea, &tea_list, 0, ""),
    OPT_HELP(""),
    OPT_HELP("Water options:"),
    OPT_SECTION(water_options),
    OPT_HOOK(opt_test_hook, "prearg", OPT_HOOK_BEFORE_ARG),
    OPT_HOOK(opt_test_hook, "preval", OPT_HOOK_BEFORE_VALUE),
    OPT_HOOK(opt_test_hook, "postval", OPT_HOOK_AFTER_VALUE),
    OPT_BOOL('H', "show-hooks", show_hooks, 0, "\tDemonstrate the hooks."),
    OPT_HELP(""),
    OPT_HELP("Configuration options:"),
    OPT_CONF_OPTIONS,
    OPT_END
  }
};

struct intnode {
  cnode n;
  int x;
};

int main(int argc UNUSED, char ** argv)
{
  cf_def_file = "etc/libucw";
  GARY_INIT(black_magic, 0);
  opt_parse(&options, argv+1);

  printf("English style: %s|", english ? "yes" : "no");
  if (sugar)
    printf("Sugar: %d teaspoons|", sugar);
  if (set != -1)
    printf("Chosen teapot: %s|", teapot_type_str[set]);
  printf("Temperature: %d%s|", temperature.value, temp_scale_str[temperature.scale]);
  printf("Verbosity: %d|", verbose);
  uint magick = GARY_SIZE(black_magic);
  for (uint i=0; i<magick; i++)
    printf("Black magic: %d|", black_magic[i]);
  printf("Prayer: %s|", pray ? "yes" : "no");
  printf("Clean: %s|", clean_pot ? "yes" : "no");
  printf("Water amount: %d|", water_amount);
  printf("Gas: %s|", with_gas ? "yes" : "no");
  printf("First tea: %s|", first_tea);
  for (int i=0; i<tea_num; i++)
    printf("Boiling a tea: %s|", tea_list[i]);

  printf("Everything OK. Bye.\n");
}
