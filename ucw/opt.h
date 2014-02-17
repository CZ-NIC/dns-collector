/*
 *	UCW Library -- Parsing of command line options
 *
 *	(c) 2013 Jan Moskyto Matejka <mq@ucw.cz>
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *	(c) 2014 Pavel Charvat <pchar@ucw.cz>
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
#define cf_def_file ucw_cf_def_file
#define cf_env_file ucw_cf_env_file
#define opt_conf_hook_internal ucw_opt_conf_hook_internal
#define opt_failure ucw_opt_failure
#define opt_handle_config ucw_opt_handle_config
#define opt_handle_dumpconfig ucw_opt_handle_dumpconfig
#define opt_handle_help ucw_opt_handle_help
#define opt_handle_set ucw_opt_handle_set
#define opt_help ucw_opt_help
#define opt_parse ucw_opt_parse
#endif

#define OPT_EXIT_BAD_ARGS 2

/***
 * [[classes]]
 * Option classes
 * --------------
 *
 * Each option belongs to one of the following classes, which define
 * the overall behavior of the option. In most cases, the classes
 * are set automatically by <<macros,declaration macros>>.
 *
 * - `OPT_CL_END`: this is not a real option class, but a signal
 *   that the list of options ends.
 * - `OPT_CL_BOOL`: a boolean option. If specified without an argument,
 *   it sets the corresponding variable to 1 (true). So does an argument of
 *   `1`, `y`, `yes`, or `true`. Conversely, an argument of `0`, `n`, `no`,
 *   or `false` sets the variable to 0 (false) and the same happens if
 *   the option is given as `--no-`'option' with no argument.
 * - `OPT_CL_STATIC`: options of this class just take a value and store
 *   it in the variable.
 * - `OPT_CL_MULTIPLE`: collect values from all occurrences of this
 *   option in a growing array (see `gary.h`).
 * - `OPT_CL_SWITCH`: a multiple-choice switch, which sets the variable
 *   to a fixed value provided in option definition.
 * - `OPT_CL_INC`: increments the variable (or decrements, if the
 *   `OPT_NEGATIVE` flag is set).
 * - `OPT_CL_CALL`: instead of setting a variable, call a function
 *   and pass the value of the option to it.
 * - `OPT_CL_SECTION`: not a real option, but an instruction to insert
 *   contents of another list of options.
 * - `OPT_CL_HELP`: no option, just print a help text.
 * - `OPT_CL_HOOK`: no option, but a definition of a <<hooks,hook>>.
 ***/

enum opt_class {
  OPT_CL_END,
  OPT_CL_BOOL,
  OPT_CL_STATIC,
  OPT_CL_MULTIPLE,
  OPT_CL_SWITCH,
  OPT_CL_INC,
  OPT_CL_CALL,
  OPT_CL_SECTION,
  OPT_CL_HELP,
  OPT_CL_HOOK,
};

/***
 * [[opt_item]]
 * Option definitions
 * ------------------
 *
 * The list of options is represented by `struct opt_section`, which points to
 * a sequence of `struct opt_item`s.
 *
 * These structures are seldom used directly -- instead, they are produced
 * by <<macros,declaration macros>>.
 ***/

/** A section of option list. **/
struct opt_section {
  struct opt_item * opt;
};

/** A definition of a single option item. **/
struct opt_item {
  const char * name;			// long name (NULL if none)
  int letter;				// short name (0 if none)
  void * ptr;				// variable to store the value to
  const char * help;			// description in --help
  union opt_union {
    struct opt_section * section;	// subsection for OPT_CL_SECTION
    int value;				// value for OPT_CL_SWITCH
    void (* call)(struct opt_item * opt, const char * value, void * data);		// function to call for OPT_CL_CALL
    void (* hook)(struct opt_item * opt, uns event, const char * value, void * data);	// function to call for OPT_CL_HOOK
    struct cf_user_type * utype;	// specification of the user-defined type for CT_USER
  } u;
  u16 flags;				// as defined below (for hooks, event mask is stored instead)
  byte cls;				// enum opt_class
  byte type;				// enum cf_type
};

/***
 * [[flags]]
 * Option flags
 * ------------
 *
 * Each option can specify a combination of the following flags.
 ***/

#define OPT_REQUIRED	    0x1		/** The option must be always present. **/
#define OPT_REQUIRED_VALUE  0x2		/** The option must have a value. **/
#define OPT_NO_VALUE	    0x4		/** The option must have no value. **/
#define OPT_MAYBE_VALUE	    0x8		/** The option may have a value. **/
#define OPT_NEGATIVE	    0x10	/** Reversing the effect of OPT_INC or saving @false into OPT_BOOL. **/
#define OPT_NO_HELP	    0x20	/** Exclude this option from the help. **/
#define OPT_LAST_ARG	    0x40	/** Stop processing arguments after this line. **/
#define OPT_SINGLE	    0x100	/** The option must appear at most once. **/
#define OPT_MULTIPLE	    0x200	/** The option may appear multiple times; will save all the values into a simple list. **/
#define OPT_SEEN_AS_LONG    0x400	// Used internally to signal that we currently process the long form of the option
#define OPT_BEFORE_CONFIG   0x800	/** The option may appear before a config file is loaded. **/
#define OPT_INTERNAL        0x4000	// Used internally to ask for passing of struct opt_context to OPT_CALL

/**
 * If none of these flags are specified, a default is chosen automatically
 * according to option class:
 *
 * - `OPT_MAYBE_VALUE` for `OPT_CL_STATIC`
 * - `OPT_REQUIRED_VALUE` for `OPT_CL_MULTIPLE`
 * - `OPT_NO_VALUE` for `OPT_CL_BOOL`, `OPT_CL_SWITCH` and `OPT_CL_INC`
 * - An error is reported in all other cases.
 **/
#define OPT_VALUE_FLAGS	    (OPT_REQUIRED_VALUE | OPT_NO_VALUE | OPT_MAYBE_VALUE)

/***
 * [[macros]]
 * Macros for declaration of options
 * ---------------------------------
 *
 * In most cases, option definitions are built using these macros.
 ***/

/** Used inside `struct opt_section` to start a list of items. **/
#define OPT_ITEMS	.opt = ( struct opt_item[] )

/** No option, just a piece of help text. **/
#define OPT_HELP(line) { .help = line, .cls = OPT_CL_HELP }

/** Standard `--help` option. **/
#define OPT_HELP_OPTION OPT_CALL(0, "help", opt_handle_help, NULL, OPT_BEFORE_CONFIG | OPT_INTERNAL | OPT_NO_VALUE, "\tShow this help")

/** Boolean option. @target should be a variable of type `int`. **/
#define OPT_BOOL(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int *), .help = desc, .flags = fl, .cls = OPT_CL_BOOL, .type = CT_INT }

/** String option. @target should be a variable of type `char *`. **/
#define OPT_STRING(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, char **), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_STRING }

/** Ordinary integer option. @target should be a variable of type `int`. **/
#define OPT_INT(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int *), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_INT }

/** Unsigned integer option. @target should be a variable of type `uint`. **/
#define OPT_UINT(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int *), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_INT }

/** 64-bit integer option. @target should be a variable of type `u64`. **/
#define OPT_U64(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, u64 *), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_U64 }

/** Floating-point option. @target should be a variable of type `double`. **/
#define OPT_DOUBLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, double *), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_DOUBLE }

/** IP address option, currently IPv4 only. @target should be a variable of type `u32`. **/
#define OPT_IP(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, u32 *), .help = desc, .flags = fl, .cls = OPT_CL_STATIC, .type = CT_IP }

/** Multi-valued string option. @target should be a growing array of `int`s. **/
#define OPT_BOOL_MULTIPLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, char ***), .help = desc, .flags = fl, .cls = OPT_CL_MULTIPLE, .type = CT_STRING }

/** Multi-valued integer option. @target should be a growing array of `int`s. **/
#define OPT_INT_MULTIPLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int **), .help = desc, .flags = fl, .cls = OPT_CL_MULTIPLE, .type = CT_INT }

/** Multi-valued unsigned integer option. @target should be a growing array of `uint`s. **/
#define OPT_UINT_MULTIPLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, uint **), .help = desc, .flags = fl, .cls = OPT_CL_MULTIPLE, .type = CT_INT }

/** Multi-valued 64-bit integer option. @target should be a growing array of `u64`s. **/
#define OPT_U64_MULTIPLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, u64 **), .help = desc, .flags = fl, .cls = OPT_CL_MULTIPLE, .type = CT_U64 }

/** Multi-valued floating-point option. @target should be a growing array of `double`s. **/
#define OPT_DOUBLE_MULTIPLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, double **), .help = desc, .flags = fl, .cls = OPT_CL_MULTIPLE, .type = CT_DOUBLE }

/** Multi-valued IPv4 address option. @target should be a growing array of `u32`s. **/
#define OPT_IP_MULTIPLE(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, u32 **), .help = desc, .flags = fl, .cls = OPT_CL_MULTIPLE, .type = CT_IP }

/** Switch option. @target should be a variable of type `int` and it will be set to the value @val. **/
#define OPT_SWITCH(shortopt, longopt, target, val, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int *), .help = desc, .flags = fl, .cls = OPT_CL_SWITCH, .type = CT_LOOKUP, .u.value = val }

/** Incrementing option. @target should be a variable of type `int`. **/
#define OPT_INC(shortopt, longopt, target, fl, desc) { .letter = shortopt, .name = longopt, .ptr = CHECK_PTR_TYPE(&target, int *), .flags = fl, .help = desc, .cls = OPT_CL_INC, .type = CT_INT }

/**
 * When this option appears, call the function @fn with parameters @item, @value, @data,
 * where @item points to the <<struct_opt_item,`struct opt_item`>> of this option,
 * @value contains the current argument of the option (NULL if there is none),
 * and @data is specified here.
 **/
#define OPT_CALL(shortopt, longopt, fn, data, fl, desc) { .letter = shortopt, .name = longopt, .ptr = data, .help = desc, .u.call = fn, .flags = fl, .cls = OPT_CL_CALL, .type = CT_USER }

/**
 * An option with user-defined syntax. @ttype is a <<conf:struct_cf_user_type,`cf_user_type`>>
 * describing the syntax, @target is a variable of the corresponding type. If the @OPT_REQUIRED_VALUE
 * flag is not set, the parser must be able to parse a NULL value.
 **/
#define OPT_USER(shortopt, longopt, target, ttype, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .u.utype = &ttype, .flags = fl, .help = desc, .cls = OPT_CL_STATIC, .type = CT_USER }

/** Multi-valued option of user-defined type. @target should be a growing array of the right kind of items. **/
#define OPT_USER_MULTIPLE(shortopt, longopt, target, ttype, fl, desc) { .letter = shortopt, .name = longopt, .ptr = &target, .u.utype = &ttype, .flags = fl, .help = desc, .cls = OPT_CL_MULTIPLE, .type = CT_USER }

/** A sub-section. **/
#define OPT_SECTION(sec) { .cls = OPT_CL_SECTION, .u.section = &sec }

/** Declares a <<hooks,hook>> to call upon any event from the specified set. **/
#define OPT_HOOK(fn, data, events) { .cls = OPT_CL_HOOK, .u.hook = fn, .flags = events, .ptr = data }

/** A terminator signalling the end of the option list. **/
#define OPT_END { .cls = OPT_CL_END }

/***
 * [[positional]]
 * Positional arguments
 * --------------------
 *
 * In addition to short and long options, the parser can also process 'positional
 * arguments', which don't start with a dash and whose meaning depends solely on
 * their position.
 *
 * Positional arguments are declared as options with short name `OPT_POSITIONAL(n)`
 * (where `n` is the position of the argument, starting with 1) and long name
 * NULL. To accept an arbitrary number of positional arguments, use
 * `OPT_POSITIONAL_TAIL` instead, which matches all arguments, for which no
 * `OPT_POSITIONAL` is defined. (In the latter case, you probably want to define
 * the argument as `OPT_MULTIPLE` or `OPT_CALL`, so that the values do not
 * overwrite each other.)
 *
 * Options and positional arguments can be mixed arbitrarily. When a `--` appears
 * as an argument, it is understood as a signal that all other arguments are
 * positional.
 *
 * `OPT_REQUIRED` can be used with positional arguments, but all required arguments
 * must come before the non-required ones. When `OPT_POSITIONAL_TAIL` is declared
 * required, it means that it must match at least once.
 *
 * Ordering of positional arguments within the list of options need not match
 * their positions. Holes in position numbering are inadvisable.
 ***/

#define OPT_POSITIONAL(n)   (OPT_POSITIONAL_TAIL+(n))
#define OPT_POSITIONAL_TAIL 128

/***
 * [[func]]
 * Functions
 * ---------
 ***/

/**
 * Parse all arguments, given in a NULL-terminated array of strings.
 *
 * Typically, this is called from `main(argc, argv)` as `opt_parse(options, argv+1)`,
 * skipping the 0th argument, which contains program name.
 *
 * Returns the number of arguments used (which need not be all of them
 * if `OPT_LAST_ARG` was encountered).
 *
 * The argument array is left untouched.
 * However, option values are not necessarily copied, the variables
 * set by the parser may point to the argument array.
 **/
int opt_parse(const struct opt_section * options, char ** argv);

/**
 * Report parsing failure, suggest `--help`, and abort the program with
 * exit code 2.
 **/
void opt_failure(const char * mesg, ...) FORMAT_CHECK(printf,1,2) NONRET;

void opt_help(const struct opt_section * sec);
void opt_handle_help(struct opt_item * opt, const char * value, void * data);

/***
 * [[conf]]
 * Cooperating with config file parser
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Parsing of command-line options and configuration files are usually
 * intertwined in a somewhat tricky way. We want to provide command-line
 * options that control the name of the configuration file, or possibly to
 * override configuration settings from the command line. On the other hand,
 * regular command-line options can refer to values loaded from the
 * program's configuration.
 *
 * To achieve this goal, the option parser is able to cooperate with the
 * config file parser. This is enabled by listing the `OPT_CONF_OPTIONS`
 * macro in the list of command-line options.
 *
 * The following options are defined for you:
 *
 * - `-C` (`--config`) to load a specific configuration file. This option
 *   suppresses loading of the default configuration, but it can be given
 *   multiple times to merge settings from several files.
 *
 * - `-S` (`--set`) to include a part of configuration inline. For example,
 *   you can use `-Ssection.item=value` to change a single configuration item.
 *
 * - `--dumpconfig` to dump the configuration to standard output and exit.
 *   (This is available only if the program is compiled with `CONFIG_UCW_DEBUG`.)
 *
 * The default configuration file (as specified by <<var_cf_def_file,`cf_def_file`>>) is loaded
 * as soon as the first option different from `-C` is encountered, unless
 * a different file has been already loaded. For this reason, `-C` must be
 * the very first argument given to the program.
 *
 * This interface supersedes <<conf:getopt_h,`cf_getopt()`>>.
 ***/

#ifdef CONFIG_UCW_DEBUG
#define OPT_CONF_OPTIONS    OPT_CONF_CONFIG, OPT_CONF_SET, OPT_CONF_DUMPCONFIG, OPT_CONF_HOOK
#else
#define OPT_CONF_OPTIONS    OPT_CONF_CONFIG, OPT_CONF_SET, OPT_CONF_HOOK
#endif

#define OPT_CONF_CONFIG	    OPT_CALL('C', "config", opt_handle_config, NULL, OPT_BEFORE_CONFIG | OPT_INTERNAL | OPT_REQUIRED_VALUE, "<file>\tOverride the default configuration file")
#define OPT_CONF_SET	    OPT_CALL('S', "set", opt_handle_set, NULL, OPT_BEFORE_CONFIG | OPT_INTERNAL | OPT_REQUIRED_VALUE, "<item>\tManual setting of a configuration item")
#define OPT_CONF_DUMPCONFIG OPT_CALL(0, "dumpconfig", opt_handle_dumpconfig, NULL, OPT_INTERNAL | OPT_NO_VALUE, "\tDump program configuration")
#define OPT_CONF_HOOK	    OPT_HOOK(opt_conf_hook_internal, NULL, OPT_HOOK_BEFORE_VALUE | OPT_HOOK_FINAL | OPT_HOOK_INTERNAL)

void opt_handle_config(struct opt_item * opt, const char * value, void * data);
void opt_handle_set(struct opt_item * opt, const char * value, void * data);
void opt_handle_dumpconfig(struct opt_item * opt, const char * value, void * data);
void opt_conf_hook_internal(struct opt_item * opt, uns event, const char * value, void * data);

// XXX: This is duplicated with <ucw/getopt.h>, but that one will hopefully go away one day.
/**
 * The name of the default configuration file (NULL if configuration has been
 * already loaded). It is initialized to `CONFIG_UCW_DEFAULT_CONFIG`, but you
 * usually want to replace it by your own config file.
 */
extern char *cf_def_file;
/**
 * Name of environment variable that can override what configuration is loaded.
 * Defaults to the value of the `CONFIG_UCW_ENV_VAR_CONFIG` compile-time option.
 **/
extern char *cf_env_file;

/***
 * [[hooks]]
 * Hooks
 * -----
 *
 * You can supply hook functions, which will be called by the parser upon various
 * events. Hooks are declared as option items of class `OPT_CL_HOOK`, whose @flags
 * field specifies a mask of events the hook wants to receive.
 *
 * Please note that the hook interface is not considered stable yet,
 * so it might change in future versions of libucw.
 *
 * The following events are defined:
 ***/

#define OPT_HOOK_BEFORE_ARG	0x1	/** Call before option parsing **/
#define OPT_HOOK_BEFORE_VALUE	0x2	/** Call before value parsing **/
#define OPT_HOOK_AFTER_VALUE	0x4	/** Call after value parsing **/
#define OPT_HOOK_FINAL		0x8	/** Call just before @opt_parse() returns **/
#define OPT_HOOK_INTERNAL       0x4000	// Used internally to ask for passing of struct opt_context

#endif
