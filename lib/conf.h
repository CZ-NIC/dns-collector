/* Reading conf files
 * Robert Spalek, (c) 2001, robert@ucw.cz
 * $Id: conf.h,v 1.1 2001/01/07 21:21:53 robert Exp $
 */

enum cftype { ct_stop, ct_int, ct_string, ct_function };

struct cfitem {
	byte *name;
	enum cftype type;
	void *var;
};

typedef byte *(*ci_func)(struct cfitem *, byte *);

void cf_register(byte *section,struct cfitem *items);
void cf_register_opts(byte *so,struct option *lo);

int cf_read(byte *filename);
void cf_read_err(byte *filename);
int cf_getopt(int argc,char * const argv[], int *longindex);

