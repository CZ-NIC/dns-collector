/*
 *	UCW Library -- Reading of configuration files
 *
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_LIB_CONF_H
#define	_LIB_CONF_H

#include <getopt.h>

/*
 * Allocation in configuration memory pool.
 */

extern struct mempool *cfpool;
void *cfg_malloc(uns size);
void *cfg_malloc_zero(uns size);
byte *cfg_strdup(byte *s);
byte *cfg_printf(char *fmt, ...) FORMAT_CHECK(printf,1,2);

/*
 * Every module places its configuration setting into some section.  Section is
 * an array of cfitem, whose first record is of type CT_SECTION and contains
 * the name of the section.  The configuration sections are registered by
 * calling cf_register().
 *
 * CT_INCOMPLETE_SECTION is identical to CT_SECTION, but when an unknown variable
 * is spotted, we ignore it instead of bailing out with an error message.
 *
 * item->var is a pointer to the destination variable or to the special parsing
 * function.
 */

enum cftype { CT_STOP, CT_SECTION, CT_INCOMPLETE_SECTION, CT_INT, CT_STRING, CT_FUNCTION, CT_DOUBLE, CT_U64 };

struct cfitem {
	byte *name;
	enum cftype type;
	void *var;
};

typedef byte *(*ci_func)(struct cfitem *, byte *);

void cf_register(struct cfitem *items);

/*
 * Direct setting of configuration items and parsing the configuration file.
 */

int cf_item_count(void);
struct cfitem *cf_get_item(byte *sect, byte *name);
byte *cf_set_item(byte *sect, byte *name, byte *value);
void cf_read(byte *filename);

/*
 * Number parsing functions which could be useful in CT_FUNCTION callbacks.
 */

byte *cf_parse_int(byte *value, uns *varp);
byte *cf_parse_u64(byte *value, u64 *varp);
byte *cf_parse_double(byte *value, double *varp);

/* 
 * Some useful parsing functions.
 */

byte *cf_parse_ip(byte **value, u32 *varp);

/*
 * When using cf_getopt, you must prefix your own short/long options by the
 * CF_(SHORT|LONG)_OPTS.
 *
 * cfdeffile contains filename of config file automatically loaded before a
 * first --set option is executed.  If none --set option occures, it will be
 * loaded after getopt returns -1 (at the end of configuration options).  It
 * will be ignored, if another config file is set by --config option at first.
 * Its initial value is DEFAULT_CONFIG from config.h, but you can override it
 * manually.
 */

#define	CF_SHORT_OPTS	"S:C:"
#define	CF_LONG_OPTS	\
	{"set",		1, 0, 'S'},\
	{"config",	1, 0, 'C'},
#define CF_NO_LONG_OPTS (const struct option []){ CF_LONG_OPTS { NULL, 0, 0, 0 } }
#define CF_USAGE_TAB ""
#define	CF_USAGE	\
"-S, --set sec.item=val\t" CF_USAGE_TAB "Manual setting of a configuration item\n\
-C, --config filename\t" CF_USAGE_TAB "Overwrite default config filename\n"

extern byte *cfdeffile;

int cf_getopt(int argc,char * const argv[],
		const char *shortopts,const struct option *longopts,
		int *longindex);

#endif
