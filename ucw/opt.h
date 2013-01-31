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

#include <stdlib.h>
#include <stdio.h>

/***
 * [[opt]]
 * Parsing of command line options
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ***/

enum opt_class {
  OPT_CL_END,	  // end of list
  OPT_CL_BOOL,	  // boolean value
  OPT_CL_STATIC,  // static value
  OPT_CL_SWITCH,  // lookup/switch
  OPT_CL_INC,	  // incremental value
  OPT_CL_USER,	  // user defined value
  OPT_CL_SECTION, // subsection
  OPT_CL_HELP,	  // help line
};

enum opt_type {
  OPT_CT_INT, OPT_CT_64, OPT_CT_DOUBLE,	// number
  OPT_CT_STRING,			// string
  OPT_CT_LOOKUP,			// lookup/switch
  OPT_CT_USER,				// user defined
};

typedef int opt_custom_parser(const char * param, void * target);

struct opt_section;
struct opt_item {
  const char letter;			// short-op
  const char *name;			// long-op
  void *ptr;				// where to save
  const char *help;			// description in --help
  union opt_union {
    struct opt_section *section;	// subsection for OPT_SECTION
    int value;				// value for OPT_SWITCH
    opt_custom_parser *parser;		// parser for OPT_USER
    const char *help2;			// second value for OPT_HELP2
  } u;
  short flags;
  enum opt_class cls;
  enum opt_type type;
};

struct opt_section {
  struct opt_item *opt;
};

#define OPT_ITEMS	.opt = ( struct opt_item[] )  /** List of sub-items. **/

/** Sub-items to be enclosed in OPT_ITEMS { } list
 *
 * OPT_SHOW_HELP declares --help and prints a line about that
 * OPT_HELP prints a line into help()
 * OPT_HELP2 prints two strings onto a line using the same tab structure as the option listing
 * OPT_BOOL declares boolean option with an auto-negation (--sth and --no-sth); may be changed by OPT_BOOL_SET_PREFIXES
 * OPT_STRING, OPT_UNS, OPT_INT declare simple string/uns/int option
 * OPT_SWITCH declares one choice of a switch statement; these have common target and different `value`s; last wins unless OPT_SINGLE is set;
 *	      parser fails if it matches an OPT_SWITCH with OPT_SINGLE set and also target set.
 *	      Target must be of signed integer type; it is set to -1 if no switch appears at the command-line.
 * OPT_USER declares a custom type of value; parser is of type opt_custom_parser
 *					     and returns 1 on success and 0 on failure
 * OPT_INC declares an incremental value like -v/--verbose
 * OPT_SECTION declares a subsection
 *
 * **/

#define OPT_SHOW_HELP(flags) OPT_USER(0, "help", *(NULL), opt_help_success2, flags, "Show this help")
#define OPT_HELP(line) OPT_HELP2(line, NULL)
#define OPT_HELP2(first, second) { .help = first, .cls = OPT_CL_HELP, .u.help2 = second }
#define OPT_BOOL(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_BOOL, .type = OPT_CT_INT }
#define OPT_STRING(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, char**), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = OPT_CT_STRING }
#define OPT_UNS(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, uns*), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = OPT_CT_INT }
#define OPT_INT(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int*), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = OPT_CT_INT }
#define OPT_SWITCH(shortopt, longopt, target, val, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int*), .help = desc, .flags = fl, .cls = OPT_CL_SWITCH, .type = OPT_CT_INT, .u.value = val }
#define OPT_USER(shortopt, longopt, target, pa, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .u.parser = pa, .flags = fl, .help = desc, .cls = OPT_CL_USER, .type = OPT_CT_USER }
#define OPT_INC(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .flags = fl, .help = desc, .cls = OPT_CL_INC, .type = OPT_CT_INT }
#define OPT_SECTION(sec) { .cls = OPT_CL_SECTION, .u.section = &sec }
#define OPT_END { .cls = OPT_CL_END }

/** Flags for the preceeding calls **/
#define OPT_REQUIRED	    0x1		// Argument must appear at the command line
#define OPT_REQUIRED_VALUE  0x2		// Argument must have a value
#define OPT_NO_VALUE	    0x4		// Argument must have no value
#define OPT_DECREMENT	    0x8		// Reversing the effect of OPT_INC
#define OPT_SINGLE	    0x10	// Argument must appear at most once
#define OPT_NO_HELP	    0x20	// Omit this line from help

extern struct opt_section * opt_section_root;
void opt_help_noexit_internal(struct opt_section * help);

static void opt_help_noexit(void) {
  opt_help_noexit_internal(opt_section_root);
}

static void opt_usage_noexit(void) {
  fprintf(stderr, "Run with argument --help for more information.\n");
}

static int opt_help_success2(const char * param UNUSED, void * target UNUSED) {
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

void opt_parse(struct opt_section * options);

#endif
