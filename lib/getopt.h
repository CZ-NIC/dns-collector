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

#include <getopt.h>

/* Safe loading and reloading of configuration files: conf-input.c */

extern byte *cf_def_file;		/* DEFAULT_CONFIG; NULL if already loaded */
int cf_reload(byte *file);
int cf_load(byte *file);
int cf_set(byte *string);

/* Direct access to configuration items: conf-intr.c */

#define CF_OPERATIONS T(CLOSE) T(SET) T(CLEAR) T(APPEND) T(PREPEND) \
  T(REMOVE) T(EDIT) T(AFTER) T(BEFORE) T(COPY)
  /* Closing brace finishes previous block.
   * Basic attributes (static, dynamic, parsed) can be used with SET.
   * Dynamic arrays can be used with SET, APPEND, PREPEND.
   * Sections can be used with SET.
   * Lists can be used with everything. */
#define T(x) OP_##x,
enum cf_operation { CF_OPERATIONS };
#undef T

struct cf_item;
byte *cf_find_item(byte *name, struct cf_item *item);
byte *cf_write_item(struct cf_item *item, enum cf_operation op, int number, byte **pars);

/* Debug dumping: conf-dump.c */

struct fastbuf;
void cf_dump_sections(struct fastbuf *fb);

/* Journaling control: conf-journal.c */

struct cf_journal_item;
struct cf_journal_item *cf_journal_new_transaction(uns new_pool);
void cf_journal_commit_transaction(uns new_pool, struct cf_journal_item *oldj);
void cf_journal_rollback_transaction(uns new_pool, struct cf_journal_item *oldj);

/*
 * cf_getopt() takes care of parsing the command-line arguments, loading the
 * default configuration file (cf_def_file) and processing configuration options.
 * The calling convention is the same as with GNU getopt_long(), but you must prefix
 * your own short/long options by the CF_(SHORT|LONG)_OPTS or pass CF_NO_LONG_OPTS
 * of there are no long options.
 *
 * The default configuration file can be overriden by the --config options,
 * which must come first. During parsing of all other options, the configuration
 * is already available.
 */

#define	CF_SHORT_OPTS	"C:S:"
#define	CF_LONG_OPTS	{"config",	1, 0, 'C'}, {"set",		1, 0, 'S'}, CF_LONG_OPTS_DEBUG
#define CF_NO_LONG_OPTS (const struct option []) { CF_LONG_OPTS { NULL, 0, 0, 0 } }
#ifndef CF_USAGE_TAB
#define CF_USAGE_TAB ""
#endif
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

// conf-input.c
int cf_getopt(int argc, char * const argv[], const char *short_opts, const struct option *long_opts, int *long_index);

#endif
