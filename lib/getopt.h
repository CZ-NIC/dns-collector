/*
 *	UCW Library -- Reading of configuration files
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_UCW_GETOPT_H
#define	_UCW_GETOPT_H

/* Safe reloading and loading of configuration files */
extern byte *cf_def_file;
int cf_reload(byte *file);
int cf_load(byte *file);
int cf_set(byte *string);

/* Direct access to configuration items */

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
struct fastbuf;
byte *cf_find_item(byte *name, struct cf_item *item);
byte *cf_write_item(struct cf_item *item, enum cf_operation op, int number, byte **pars);
void cf_dump_sections(struct fastbuf *fb);

/*
 * When using cf_get_opt(), you must prefix your own short/long options by the
 * CF_(SHORT|LONG)_OPTS.
 *
 * cf_def_file contains the name of a configuration file that will be
 * automatically loaded before the first --set option is executed.  If no --set
 * option occurs, it will be loaded after getopt() returns -1 (i.e. at the end
 * of the configuration options).  cf_def_file will be ignored if another
 * configuration file has already been loaded using the --config option.  The
 * initial value of cf_def_file is DEFAULT_CONFIG from config.h, but you can
 * override it manually before calling cf_get_opt().
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

#include <getopt.h>
int cf_getopt(int argc, char * const argv[], const char *short_opts, const struct option *long_opts, int *long_index);

#endif
