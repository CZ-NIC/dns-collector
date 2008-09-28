/*
 *	UCW Library -- Parsing of configuration and command-line options
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_UCW_GETOPT_H
#define	_UCW_GETOPT_H

#ifdef CONFIG_OWN_GETOPT
#include "ucw/getopt/getopt-sh.h"
#else
#include <getopt.h>
#endif

void reset_getopt(void);	/** If you want to start parsing of the arguments from the first one again. **/

/***
 * [[conf_load]]
 * Safe configuration loading
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * These functions can be used to to safely load or reload configuration.
 */

/**
 * The default config (DEFAULT_CONFIG config option) or NULL if already loaded.
 * You can set it to something else manually.
 */
extern char *cf_def_file;		/* DEFAULT_CONFIG; NULL if already loaded */
extern char *cf_env_file;		/* ENV_VAR_CONFIG */
int cf_reload(const char *file);	/** Reload configuration from @file, replace the old one. **/
int cf_load(const char *file);		/** Load configuration from @file. **/
/**
 * Parse some part of configuration passed in @string.
 * The syntax is the same as in the <<config:,configuration file>>.
 **/
int cf_set(const char *string);

/***
 * [[conf_direct]]
 * Direct access
 * ~~~~~~~~~~~~~
 *
 * Direct access to configuration items.
 * You probably should not need this.
 ***/

#define CF_OPERATIONS T(CLOSE) T(SET) T(CLEAR) T(ALL) \
  T(APPEND) T(PREPEND) T(REMOVE) T(EDIT) T(AFTER) T(BEFORE) T(COPY)
  /* Closing brace finishes previous block.
   * Basic attributes (static, dynamic, parsed) can be used with SET.
   * Dynamic arrays can be used with SET, APPEND, PREPEND.
   * Sections can be used with SET.
   * Lists can be used with everything. */
#define T(x) OP_##x,
enum cf_operation { CF_OPERATIONS };
#undef T

struct cf_item;
/**
 * Searches for a configuration item called @name.
 * If it is found, it is copied into @item and NULL is returned.
 * Otherwise, an error is returned and @item is zeroed.
 **/
char *cf_find_item(const char *name, struct cf_item *item);
// TODO What does this do?
char *cf_write_item(struct cf_item *item, enum cf_operation op, int number, char **pars);

/***
 * [[conf_dump]]
 * Debug dumping
 * ~~~~~~~~~~~~~
 ***/

struct fastbuf;
/**
 * Take everything and write it into @fb.
 **/
void cf_dump_sections(struct fastbuf *fb);

/***
 * [[conf_journal]]
 * Journaling control
 * ~~~~~~~~~~~~~~~~~~
 ***/

struct cf_journal_item;
// TODO
struct cf_journal_item *cf_journal_new_transaction(uns new_pool);
void cf_journal_commit_transaction(uns new_pool, struct cf_journal_item *oldj);
void cf_journal_rollback_transaction(uns new_pool, struct cf_journal_item *oldj);

/***
 * [[conf_getopt]]
 * Loading by @cf_getopt()
 * ~~~~~~~~~~~~~~~~~~~~~~~
 ***/

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

#ifdef CONFIG_DEBUG
#define CF_LONG_OPTS_DEBUG { "dumpconfig", 0, 0, 0x64436667 } ,
#define CF_USAGE_DEBUG "    --dumpconfig\t" CF_USAGE_TAB "Dump program configuration\n"
#else
#define CF_LONG_OPTS_DEBUG
#define CF_USAGE_DEBUG
#endif

/***
 * Takes care of parsing the command-line arguments, loading the
 * default configuration file (<<var_cf_def_file,`cf_def_file`>>) and processing
 * configuration options. The calling convention is the same as with GNU getopt_long(),
 * but you must prefix your own short/long options by the
 * <<def_CF_LONG_OPTS,`CF_LONG_OPTS`>> or <<def_CF_SHORT_OPTS,`CF_SHORT_OPTS`>>or
 * pass <<def_CF_NO_LONG_OPTS,`CF_NO_LONG_OPTS`>> if there are no long options.
 *
 * The default configuration file can be overwriten by the --config options,
 * which must come first. During parsing of all other options, the configuration
 * is already available.
 ***/
int cf_getopt(int argc, char * const argv[], const char *short_opts, const struct option *long_opts, int *long_index);

#endif
