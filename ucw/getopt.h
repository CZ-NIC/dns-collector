/*
 *	UCW Library -- Parsing of configuration and command-line options
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_UCW_GETOPT_H
#define	_UCW_GETOPT_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define cf_def_file ucw_cf_def_file
#define cf_env_file ucw_cf_env_file
#define cf_getopt ucw_cf_getopt
#define reset_getopt ucw_reset_getopt
#endif

#ifdef CONFIG_UCW_OWN_GETOPT
#include <ucw/getopt/getopt-sh.h>
#else
#include <getopt.h>
#endif

/***
 * [[conf_getopt]]
 * Loading by @cf_getopt()
 * ~~~~~~~~~~~~~~~~~~~~~~~
 ***/

/**
 * The default config (as set by `CONFIG_UCW_DEFAULT_CONFIG`) or NULL if already loaded.
 * You can set it to something else manually.
 */
extern char *cf_def_file;
/**
 * Name of environment variable that can override what configuration is loaded.
 * Defaults to `CONFIG_UCW_ENV_VAR_CONFIG`.
 **/
extern char *cf_env_file;
/**
 * Short options for loading configuration by @cf_getopt().
 * Prepend to your own options.
 **/
#define	CF_SHORT_OPTS	"C:S:"
/**
 * Long options for loading configuration by @cf_getopt().
 * Prepend to your own options.
 **/
#define	CF_LONG_OPTS	{"config",	1, 0, 'C'}, {"set",		1, 0, 'S'}, CF_LONG_OPTS_DEBUG
/**
 * Use this constant as @long_opts parameter of @cf_getopt() if you do
 * not have any long options in your program.
 **/
#define CF_NO_LONG_OPTS (const struct option []) { CF_LONG_OPTS { NULL, 0, 0, 0 } }
#ifndef CF_USAGE_TAB
#define CF_USAGE_TAB ""
#endif
/**
 * This macro provides text describing usage of the configuration
 * loading options. Concatenate with description of your options and
 * write to the user, if he/she provides invalid options.
 **/
#define	CF_USAGE	\
"-C, --config filename\t" CF_USAGE_TAB "Override the default configuration file\n\
-S, --set sec.item=val\t" CF_USAGE_TAB "Manual setting of a configuration item\n" CF_USAGE_DEBUG

#ifdef CONFIG_UCW_DEBUG
#define CF_LONG_OPTS_DEBUG { "dumpconfig", 0, 0, 0x64436667 } ,
#define CF_USAGE_DEBUG "    --dumpconfig\t" CF_USAGE_TAB "Dump program configuration\n"
#else
#define CF_LONG_OPTS_DEBUG
#define CF_USAGE_DEBUG
#endif

/**
 * Takes care of parsing the command-line arguments, loading the
 * default configuration file (<<var_cf_def_file,`cf_def_file`>>) and processing
 * configuration options. The calling convention is the same as with GNU getopt_long(),
 * but you must prefix your own short/long options by the
 * <<def_CF_LONG_OPTS,`CF_LONG_OPTS`>> or <<def_CF_SHORT_OPTS,`CF_SHORT_OPTS`>> or
 * pass <<def_CF_NO_LONG_OPTS,`CF_NO_LONG_OPTS`>> if there are no long options.
 *
 * The default configuration file can be overwritten by the --config options,
 * which must come first. During parsing of all other options, the configuration
 * is already available.
 **/
int cf_getopt(int argc, char * const argv[], const char *short_opts, const struct option *long_opts, int *long_index);

void reset_getopt(void);	/** If you want to start parsing of the arguments from the first one again. **/

#endif
