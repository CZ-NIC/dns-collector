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
#include <ucw/stkstring.h>

#include <alloca.h>

struct opt_section * opt_section_root;

void opt_help_noexit_internal(struct opt_section * help) {
  uns first_column = 0;
  for (struct opt_item * item = help->opt; item->cls != OPT_CL_END; item++) {
    if (item->flags | OPT_NO_HELP) continue;
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
    } else { // will write sth like "--exclusive"
      linelen = strlen("--") + strlen(item->name);
    }

    if (item->flags | OPT_REQUIRED_VALUE) {
      linelen += strlen("=value");
    } else if (!(item->flags | OPT_NO_VALUE)) {
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

#define VAL(it) ((it->flags | OPT_REQUIRED_VALUE) ? "=value" : ((it->flags | OPT_NO_VALUE) ? "" : "(=value)"))
  for (struct opt_item * item = help->opt; item->cls != OPT_CL_END; item++) {
    if (item->flags | OPT_NO_HELP) continue;
    
    if (item->cls == OPT_CL_HELP) {
      fprintf(stderr, "%s", item->help);
      if (item->u.help2 == NULL)
	fprintf(stderr, "\n");
      else
	fprintf(stderr, "%s %s\n", spaces + strlen(item->help), item->u.help2);
    } else if (item->cls == OPT_CL_SECTION) {
      opt_help_noexit_internal(item->u.section);
    } else if (item->letter) {
      sprintf(buf, "-%c, --%s%s", item->letter, item->name, VAL(item));
      fprintf(stderr, "%s%s %s\n", buf, spaces + strlen(buf), item->help);
    } else {
      sprintf(buf, "--%s%s", item->name, VAL(item));
      fprintf(stderr, "%s%s %s\n", buf, spaces + strlen(buf), item->help);
    }
  }
}

void opt_parse(struct opt_section * options) {
  opt_section_root = options;
  opt_help();
}

#ifdef TEST

static enum TEAPOT_TYPE {
  TEAPOT_STANDARD = 0,
  TEAPOT_EXCLUSIVE,
  TEAPOT_GLASS,
  TEAPOT_HANDS,
  TEAPOT_UNDEFINED = -1
} set = TEAPOT_UNDEFINED;

static int english = 0;
static char * name = NULL;
static uns sugar = 0;
static uns verbose = 1;
static int with_gas = 0;
static uns black_magic = 0;
static int pray = 0;
static uns water_amount = 0;

static struct teapot_temperature {
  enum {
    TEMP_CELSIUS,
    TEMP_FAHRENHEIT,
    TEMP_KELVIN,
    TEMP_REAUMUR
  } scale;
  int value;
} temperature;

static int parse_temperature(const char * param, void * target) {
  return 1;
}

static struct opt_section water_options = {
  OPT_ITEMS {
    OPT_UNS('w', "water", water_amount, OPT_REQUIRED | OPT_REQUIRED_VALUE, "Amount of water (in mls)"),
    OPT_BOOL('G', "with-gas", with_gas, OPT_NO_VALUE, "Use water with gas"),
    OPT_END
  }
};

static struct opt_section help = {
  OPT_ITEMS {
    OPT_HELP("A simple tea boiling console."),
    OPT_HELP("Options:"),
    OPT_SHOW_HELP(0),
    OPT_BOOL('e', "english-style", english, 0, "English style (with milk)"),
    OPT_STRING('n', "name", name, OPT_REQUIRED | OPT_REQUIRED_VALUE, "Name of the tea to be prepared"),
    OPT_UNS('s', "sugar", sugar, OPT_REQUIRED_VALUE, "Amount of sugar (in teaspoons)"),
    OPT_SWITCH(0, "standard-set", set, TEAPOT_STANDARD, 0, "Standard teapot"),
    OPT_SWITCH('x', "exclusive-set", set, TEAPOT_EXCLUSIVE, 0, "Exclusive teapot"),
    OPT_SWITCH('g', "glass-set", set, TEAPOT_GLASS, 0, "Transparent glass teapot"),
    OPT_SWITCH('h', "hands", set, TEAPOT_HANDS, 0, "Use user's hands as a teapot (a bit dangerous)"),
    OPT_USER('t', "temperature", temperature, parse_temperature, OPT_REQUIRED_VALUE, "Wanted final temperature of the tea to be served"),
    OPT_HELP2("", "Supported scales: Celsius [60C], Fahrenheit [140F],"),
    OPT_HELP2("", "                  Kelvin [350K], Rankine [600R] and Reaumur [50Re]"),
    OPT_INC('v', "verbose", verbose, 0, "Verbose (the more -v, the more verbose)"),
    OPT_INC('q', "quiet", verbose, OPT_DECREMENT, "Quiet (the more -q, the more quiet)"),
    OPT_UNS('b', "black-magic", black_magic, 0, "Use black magic to make the tea extraordinary delicious"),
    OPT_BOOL('p', "pray", pray, 0, "Pray before boiling"),
    OPT_HELP(""),
    OPT_HELP("Water options:"),
    OPT_SECTION(water_options),
    OPT_END
  }
};

int main(int argc, char ** argv)
{
  opt_parse(&help);
}

#endif
