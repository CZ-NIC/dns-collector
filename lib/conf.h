/*
 *	Sherlock Library -- Reading configuration files
 *
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 */

#include <getopt.h>

/*
 * Allocation in configuration memory pool.
 */

void *cfg_malloc(uns size);
byte *cfg_stralloc(byte *s);

/*
 * Every module places its configuration setting into some section.  Section is
 * an array of cfitem, whose first record is of type CT_SECTION and contains
 * the name of the section.  The configuration sections are registered by
 * calling cf_register().
 *
 * item->var is a pointer to the destination variable or to the special parsing
 * function.
 */

enum cftype { CT_STOP, CT_SECTION, CT_INT, CT_STRING, CT_FUNCTION };

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

struct cfitem *cf_get_item(byte *sect, byte *name);
byte *cf_set_item(byte *sect, byte *name, byte *value);
void cf_read(byte *filename);

/*
 * When using cf_getopt, you must prefix your own short/long options by the
 * CF_(SHORT|LONG)_OPTS.
 */

#define	CF_SHORT_OPTS	"S:C:"
#define	CF_LONG_OPTS	\
	{"set",		1, 0, 'S'},\
	{"config",	1, 0, 'C'},
#define CF_NO_LONG_OPTS (const struct option []){ CF_LONG_OPTS { NULL, 0, 0, 0 } }

int cf_getopt(int argc,char * const argv[],
		const char *shortopts,const struct option *longopts,
		int *longindex);

