/*
 *	UCW Library -- Parsing of command line options
 *
 *	(c) 2013 Jan Moskyto Matejka <mq@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_OPT_H
#define _UCW_OPT_H

#include <ucw/lib.h>
#include <ucw/conf.h>

#include <stdlib.h>
#include <stdio.h>

/***
 * [[opt]]
 ***/

enum opt_class {
  OPT_CL_END,	  // end of list
  OPT_CL_BOOL,	  // boolean value
  OPT_CL_STATIC,  // static value
  OPT_CL_SWITCH,  // lookup/switch
  OPT_CL_INC,	  // incremental value
  OPT_CL_CALL,	  // call a function
  OPT_CL_USER,	  // user defined value
  OPT_CL_SECTION, // subsection
  OPT_CL_HELP,	  // help line
};

typedef void opt_custom_function(const char ** param);

struct opt_section;
struct opt_item {
  const char letter;			// short-op
  const char * name;			// long-op
  void * ptr;				// where to save
  const char * help;			// description in --help
  union opt_union {
    struct opt_section * section;	// subsection for OPT_SECTION
    int value;				// value for OPT_SWITCH
    const char * help2;			// second value for OPT_HELP2
    int (* call)(const char ** param);	// function to call for OPT_CALL
    struct cf_user_type * utype;		// specification of the user-defined type
  } u;
  short flags;
  enum opt_class cls;
  enum cf_type type;
};

struct opt_section {
  struct opt_item * opt;
};

#define OPT_ITEMS	.opt = ( struct opt_item[] )  /** List of sub-items. **/

/***
 * Sub-items to be enclosed in OPT_ITEMS { } list
 * ----------------------------------------------
 *
 * OPT_SHOW_HELP declares --help and prints a line about that
 * OPT_HELP prints a line into help()
 * OPT_HELP2 prints two strings onto a line using the same tab structure as the option listing
 * OPT_BOOL declares boolean option with an auto-negation (--sth and --no-sth); may be changed by OPT_BOOL_SET_PREFIXES
 * OPT_STRING, OPT_UNS, OPT_INT declare simple string/uns/int option
 * OPT_SWITCH declares one choice of a switch statement; these have common target and different `value`s; last wins unless OPT_SINGLE is set;
 *	      parser fails if it matches an OPT_SWITCH with OPT_SINGLE set and also target set.
 *	      Target must be of signed integer type; it is set to -1 if no switch appears at the command-line.
 * OPT_CALL calls the given function with all the remaining command line, it returns the number of arguments to be skipped.
 * OPT_USER declares a custom type of value; parser is of type opt_custom_parser
 *					     and returns 1 on success and 0 on failure
 * OPT_INC declares an incremental value like -v/--verbose
 * OPT_SECTION declares a subsection
 *
 ***/

#define OPT_SHOW_HELP OPT_CALL(0, "help", opt_show_help_internal, OPT_NO_VALUE, "Show this help")
#define OPT_HELP(line) OPT_HELP2(line, NULL)
#define OPT_HELP2(first, second) { .help = first, .cls = OPT_CL_HELP, .u.help2 = second }
#define OPT_BOOL(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_BOOL, .type = CT_INT }
#define OPT_STRING(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, char **), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_STRING }
#define OPT_UNS(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, uns *), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_INT }
#define OPT_INT(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int *), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_INT }
#define OPT_SWITCH(shortopt, longopt, target, val, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int *), .help = desc, .flags = fl, .cls = OPT_CL_SWITCH, .type = CT_LOOKUP, .u.value = val }
#define OPT_CALL(shortopt, longopt, fn, fl, desc) { .letter = shortopt, .name = longopt, .ptr = NULL, .help = desc, .u.call = fn, .flags = fl, .cls = OPT_CL_CALL, .type = CT_USER }
#define OPT_USER(shortopt, longopt, target, ttype, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .u.utype = &ttype, .flags = fl, .help = desc, .cls = OPT_CL_USER, .type = CT_USER }
#define OPT_INC(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .flags = fl, .help = desc, .cls = OPT_CL_INC, .type = CT_INT }
#define OPT_SECTION(sec) { .cls = OPT_CL_SECTION, .u.section = &sec }
#define OPT_END { .cls = OPT_CL_END }

/***
 * Flags for the preceeding calls
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ***/

#define OPT_REQUIRED	    0x1		/** Argument must appear at the command line **/
#define OPT_REQUIRED_VALUE  0x2		/** Argument must have a value **/
#define OPT_NO_VALUE	    0x4		/** Argument must have no value **/
#define OPT_MAYBE_VALUE	    0x8		/** Argument may have a value **/
#define OPT_VALUE_FLAGS	    (OPT_REQUIRED_VALUE | OPT_NO_VALUE | OPT_MAYBE_VALUE)
#define OPT_DECREMENT	    0x10	/** Reversing the effect of OPT_INC **/
#define OPT_SINGLE	    0x20	/** Argument must appear at most once **/
#define OPT_NO_HELP	    0x40	/** Omit this line from help **/

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

extern struct opt_section * opt_section_root;
void opt_help_noexit_internal(struct opt_section * help);

static void opt_help_noexit(void) {
  opt_help_noexit_internal(opt_section_root);
}

static void opt_usage_noexit(void) {
  fprintf(stderr, "Run with argument --help for more information.\n");
}

static int opt_show_help_internal(const char ** param UNUSED) {
  opt_help_noexit();
  exit(0);
}

static void opt_help(void) {
  opt_help_noexit();
  exit(1);
}

static void opt_usage(void) {
  opt_usage_noexit();
  exit(1);
}

/**
 * Init the opt engine.
 **/
void opt_init(struct opt_section * options);

/**
 * Parse all the arguments.
 * Returns the number of positional arguments and an array of them in @posargs.
 **/
int opt_parse(char ** argv, char *** posargs);

/**
 * Parse all the arguments until first positional argument is found.
 * Returns the position of that argument in argv.
 * On next call of this function, start from the next item in argv.
 * Iterate this function to get all the positional arguments.
 **/
int opt_get(char ** argv);

#endif
