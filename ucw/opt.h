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

#ifdef CONFIG_UCW_CLEAN_ABI
#define opt_conf_hook_internal ucw_opt_conf_hook_internal
#define opt_conf_internal ucw_opt_conf_internal
#define opt_conf_parsed_count ucw_opt_conf_parsed_count
#define opt_help_internal ucw_opt_help_internal
#define opt_parse ucw_opt_parse
#define opt_parsed_count ucw_opt_parsed_count
#define opt_section_root ucw_opt_section_root
#endif

#define OPT_EXIT_BAD_ARGS 2

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
  OPT_CL_HOOK,	  // hook
};

struct opt_section;
struct opt_item {
  const char * name;			// long-op
  int letter;				// short-op
  void * ptr;				// where to save
  const char * help;			// description in --help
  union opt_union {
    struct opt_section * section;	// subsection for OPT_SECTION
    int value;				// value for OPT_SWITCH
    void (* call)(struct opt_item * opt, const char * value, void * data);  // function to call for OPT_CALL
    struct cf_user_type * utype;	// specification of the user-defined type
  } u;
  u16 flags;
  byte cls;				// enum opt_class
  byte type;				// enum cf_type
};

struct opt_section {
  struct opt_item * opt;
};

#define OPT_ITEMS	.opt = ( struct opt_item[] )  /** List of sub-items. **/

/***
 * Sub-items to be enclosed in OPT_ITEMS { } list
 * ----------------------------------------------
 *
 *  OPT_HELP_OPTION declares --help and prints a line about that
 *  OPT_HELP prints a line into help
 *  OPT_HELP2 prints two strings onto a line using the same tab structure as the option listing
 *  OPT_BOOL declares boolean option with an auto-negation (--sth and --no-sth). It's also possible to write --sth=y/yes/true/1/n/no/false/0.
 *  OPT_STRING, OPT_UNS, OPT_INT declare simple string/uns/int option
 *  OPT_SWITCH declares one choice of a switch statement; these have common target and different `value`s; last wins unless OPT_SINGLE is set;
 *	       parser fails if it matches an OPT_SWITCH with OPT_SINGLE set and also target set.
 *	       Target must be of signed integer type; it is set to -1 if no switch appears at the command-line.
 *  OPT_CALL calls the given function with an argument, giving also the opt_item structure and some custom data.
 *  OPT_HOOK is called at the specified place: before option parsing, before value parsing and after value parsing as specified in @flags;
 *	       OPT_HOOK_BEFORE_ARG gets @opt and @value set to NULL;
 *	       OPT_HOOK_BEFORE_VALUE gets both @opt and @value set.
 *	       OPT_HOOK_AFTER_VALUE gets both @opt and @value set.
 *  OPT_USER declares a custom type of value defined by the given @cf_user_type in @ttype
 *  OPT_INC declares an incremental value like -v/--verbose
 *  OPT_SECTION declares a subsection
 *
 ***/

#define OPT_HELP_OPTION OPT_CALL(0, "help", opt_show_help_internal, NULL, OPT_NO_VALUE, "Show this help")
#define OPT_HELP(line) { .help = line, .cls = OPT_CL_HELP }
#define OPT_BOOL(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_BOOL, .type = CT_INT }
#define OPT_STRING(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_STRING }
// FIXME: U64 and DOUBLE are not described in the comment above
#define OPT_U64(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_U64 }
#define OPT_INT(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_INT }
#define OPT_DOUBLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_DOUBLE }
// FIXME: Does IP deserve a basic type? Wouldn't a pre-defined user type be better?
// Especially, this would provide an easy extension for IPv6.
#define OPT_IP(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_IP }
// FIXME: Semantics not clear from the description above
#define OPT_SWITCH(shortopt, longopt, target, val, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .help = desc, .flags = fl, .cls = OPT_CL_SWITCH, .type = CT_LOOKUP, .u.value = val }
#define OPT_CALL(shortopt, longopt, fn, data, fl, desc) { .letter = shortopt, .name = longopt, .ptr = data, .help = desc, .u.call = fn, .flags = fl, .cls = OPT_CL_CALL, .type = CT_USER }
#define OPT_USER(shortopt, longopt, target, ttype, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .u.utype = &ttype, .flags = fl, .help = desc, .cls = OPT_CL_USER, .type = CT_USER }
// FIXME: Check that the target is of the right type (likewise in other statically typed options)
#define OPT_INC(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .flags = fl, .help = desc, .cls = OPT_CL_INC, .type = CT_INT }
#define OPT_SECTION(sec) { .cls = OPT_CL_SECTION, .u.section = &sec }
#define OPT_HOOK(fn, data, fl) { .cls = OPT_CL_HOOK, .u.call = fn, .flags = OPT_NO_HELP | fl, .ptr = data }
#define OPT_END { .cls = OPT_CL_END }

/***
 * UCW Conf options
 * ~~~~~~~~~~~~~~~~
 *
 * OPT_CONF_OPTIONS declares -C and -S as described in @getopt.h
 ***/

#ifdef CONFIG_UCW_DEBUG
#define OPT_CONF_OPTIONS    OPT_CONF_CONFIG, OPT_CONF_SET, OPT_CONF_DUMPCONFIG, OPT_CONF_HOOK
#else
#define OPT_CONF_OPTIONS    OPT_CONF_CONFIG, OPT_CONF_SET, OPT_CONF_HOOK
#endif

#define OPT_CONF_CONFIG	    OPT_CALL('C', "config", opt_conf_internal, NULL, OPT_REQUIRED_VALUE, "Override the default configuration file")
#define OPT_CONF_SET	    OPT_CALL('S', "set", opt_conf_internal, NULL, OPT_REQUIRED_VALUE, "Manual setting of a configuration item")
#define OPT_CONF_DUMPCONFIG OPT_CALL(0, "dumpconfig", opt_conf_internal, NULL, OPT_NO_VALUE, "Dump program configuration")
#define OPT_CONF_HOOK	    OPT_HOOK(opt_conf_hook_internal, NULL, OPT_HOOK_BEFORE_VALUE)

void opt_conf_internal(struct opt_item * opt, const char * value, void * data);
void opt_conf_hook_internal(struct opt_item * opt, const char * value, void * data);

extern int opt_parsed_count;	    /** How many opts have been already parsed. **/
extern int opt_conf_parsed_count;

/***
 * Predefined shortopt arguments
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * for the preceding calls if positional args wanted.
 * Arguments are processed in the order of the numbers given to them. There must be first
 * the args with OPT_REQUIRED (see below) and after them the args without OPT_REQUIRED, no mixing.
 * You may define a catch-all option as OPT_POSITIONAL_TAIL. After this, no positional arg is allowed.
 * You may shuffle the positional arguments in any way in the opt sections but the numbering must obey
 * the rules given here.
 ***/
// FIXME: The previous paragraph is almost incomprehensible

// FIXME: Is numbering from 1 natural here?
// FIXME: Are there any rules for mixing of positional arguments with options?
#define OPT_POSITIONAL(n)   (OPT_POSITIONAL_TAIL+(n))
#define OPT_POSITIONAL_TAIL 256


/***
 * Flags for the preceding calls
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ***/

#define OPT_REQUIRED	    0x1		/** Argument must appear at the command line **/
#define OPT_REQUIRED_VALUE  0x2		/** Argument must have a value **/
#define OPT_NO_VALUE	    0x4		/** Argument must have no value **/
#define OPT_MAYBE_VALUE	    0x8		/** Argument may have a value **/
#define OPT_VALUE_FLAGS	    (OPT_REQUIRED_VALUE | OPT_NO_VALUE | OPT_MAYBE_VALUE)
#define OPT_NEGATIVE	    0x10	/** Reversing the effect of OPT_INC or saving @false into OPT_BOOL **/
#define OPT_NO_HELP	    0x20	/** Omit this line from help **/
#define OPT_LAST_ARG	    0x40	/** Stop processing argv after this line **/
#define OPT_SINGLE	    0x100	/** Argument must appear at most once **/
#define OPT_MULTIPLE	    0x200	/** Argument may appear any time; will save all the values into a simple list **/
#define OPT_HOOK_BEFORE_ARG	0x1000	/** Call before option parsing **/
#define OPT_HOOK_BEFORE_VALUE	0x2000	/** Call before value parsing **/
#define OPT_HOOK_AFTER_VALUE	0x4000  /** Call after value parsing **/


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

extern const struct opt_section * opt_section_root;
void opt_help_internal(const struct opt_section * help);

static void opt_help(void) {
  opt_help_internal(opt_section_root);
}

static void opt_usage(void) {
  fprintf(stderr, "Run with argument --help for more information.\n");
}

static void opt_show_help_internal(struct opt_item * opt UNUSED, const char * value UNUSED, void * data UNUSED) {
  opt_help();
  exit(0);
}

/**
 * Parse all the arguments.
 **/
void opt_parse(const struct opt_section * options, char ** argv);
// FIXME: When parsing finishes (possibly due to OPT_LAST_ARG), what is guaranteed
// about the state of argv[]?

#endif
