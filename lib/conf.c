/*
 *	Sherlock Library -- Reading of configuration files
 *
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/chartype.h"
#include "lib/fastbuf.h"
#include "lib/pools.h"

#include "lib/conf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#define	BUFFER		1024
#define	MAX_LEVEL	8

static struct cfitem *cfsection;
struct mempool *cfpool;

byte *cfdeffile = DEFAULT_CONFIG;

static void CONSTRUCTOR
conf_init(void)
{
	cfpool = mp_new(4096);
}

void *
cfg_malloc(uns size)
{
	return mp_alloc(cfpool, size);
}

byte *
cfg_stralloc(byte *s)
{
	uns l = strlen(s);
	byte *k = cfg_malloc(l + 1);
	strcpy(k, s);
	return k;
}

void cf_register(struct cfitem *items)
{
	if(items[0].type!=CT_SECTION && items[0].type!=CT_INCOMPLETE_SECTION)
		die("cf_register: Invalid section type");
	items[0].var=cfsection;
	cfsection=items;
}

int cf_item_count(void)
{
	struct cfitem *sect, *item;
	int count = 0;
	for (sect = cfsection; sect; sect = sect->var)
		for (item = sect+1; item->type; item++)
			count++;
	return count;
}

struct cfitem *cf_get_item(byte *sect, byte *name)
{
	struct cfitem *item, *section;

	item=cfsection;
	while(item && strcasecmp(item->name,sect))
		item=item->var;
	if(!item)	/* unknown section */
		return NULL;
	section = item;

	for(item++; item->type && strcasecmp(item->name,name); item++);
	if (!item->type && section->type == CT_INCOMPLETE_SECTION)
		return NULL;

	return item;	/* item->type == 0 if not found */
}

struct unit {
	uns name;			/* One-letter name of the unit */
	uns num, den;			/* Fraction */
};

static const struct unit units[] = {
	{ 'd', 86400, 1 },
	{ 'h', 1440, 1 },
	{ 'k', 1000, 1 },
	{ 'm', 1000000, 1 },
	{ 'g', 1000000000, 1 },
	{ 'K', 1024, 1 },
	{ 'M', 1048576, 1 },
	{ 'G', 1073741824, 1 },
	{ '%', 1, 100 },
	{ 0, 0, 0 }
};

static const struct unit *cf_lookup_unit(byte *value, byte *end, char **msg)
{
	if (end && *end) {
		if (end == value || end[1] || *end >= '0' && *end <= '9')
			*msg = "Invalid number";
		else {
			for (const struct unit *u=units; u->name; u++)
				if (u->name == *end)
					return u;
			*msg = "Invalid unit";
		}
	}
	return NULL;
}

static char cf_rngerr[] = "Number out of range";

byte *cf_parse_int(byte *value, uns *varp)
{
	char *msg = NULL;
	const struct unit *u;

	if (!*value)
		msg = "Missing number";
	else {
		errno = 0;
		char *end;
		uns x = strtoul(value, &end, 0);
		if (errno == ERANGE)
			msg = cf_rngerr;
		else if (u = cf_lookup_unit(value, end, &msg)) {
			u64 y = (u64)x * u->num;
			if (y % u->den)
				msg = "Number is not an integer";
			else {
				y /= u->den;
				if (y > 0xffffffff)
					msg = cf_rngerr;
				*varp = y;
			}
		} else
			*varp = x;
	}
	return msg;
}

byte *cf_parse_u64(byte *value, u64 *varp)
{
	char *msg = NULL;
	const struct unit *u;

	if (!*value)
		msg = "Missing number";
	else {
		errno = 0;
		char *end;
		u64 x = strtoull(value, &end, 0);
		if (errno == ERANGE)
			msg = cf_rngerr;
		else if (u = cf_lookup_unit(value, end, &msg)) {
			if (x > ~(u64)0 / u->num)
				msg = "Number out of range";
			else {
				x *= u->num;
				if (x % u->den)
					msg = "Number is not an integer";
				else
					*varp = x / u->den;
			}
		} else
			*varp = x;
	}
	return msg;
}

byte *cf_parse_double(byte *value, double *varp)
{
	char *msg = NULL;
	const struct unit *u;

	if (!*value)
		msg = "Missing number";
	else {
		errno = 0;
		char *end;
		double x = strtoul(value, &end, 0);
		if (errno == ERANGE)
			msg = cf_rngerr;
		else if (u = cf_lookup_unit(value, end, &msg))
			*varp = x * u->num / u->den;
		else
			*varp = x;
	}
	return msg;
}

byte *cf_set_item(byte *sect, byte *name, byte *value)
{
	struct cfitem *item;
	byte *msg=NULL;

	if (!*sect)
		return "Empty section name";
	item=cf_get_item(sect,name);
	if(!item)	/* ignore unknown section */
		return NULL;

	switch(item->type){
		case CT_INT:
			msg = cf_parse_int(value, (uns *) item->var);
			break;
		case CT_STRING:
			*((byte **) item->var) = cfg_stralloc(value);
			break;
		case CT_FUNCTION:
			msg = ((ci_func) item->var)(item, cfg_stralloc(value));
			break;
		case CT_DOUBLE:
			msg = cf_parse_double(value, (double *) item->var);
			break;
		case CT_U64:
			msg = cf_parse_u64(value, (u64 *) item->var);
			break;
		default:
			msg = "Unknown keyword";
	}

	return msg;
}

static int cf_subread(byte *filename,int level)
{
	int fd;
	struct fastbuf *b;
	byte def_section[BUFFER];
	int line;
	byte *msg=NULL;

	if(level>=MAX_LEVEL){
		log(L_ERROR,"Too many (%d) nested files when reading %s",level,filename);
		return 0;
	}
		
	fd=open(filename,O_RDONLY, 0666);
	if(fd<0){
		log(L_ERROR,"Cannot open configuration file %s: %m",filename);
		return 0;
	}
	b=bfdopen(fd,4096);

	def_section[0]=0;
	line=0;
	while(1){
		byte buf[BUFFER];
		byte *c;

		if(!bgets(b,buf,BUFFER))
			break;
		line++;

		c=buf+strlen(buf);
		while(c>buf && Cspace(c[-1]))
			*--c=0;
		c=buf;
		while(*c && Cspace(*c))
			c++;
		if(!*c || *c=='#')
			continue;

		if(*c=='['){
			strcpy(def_section,c+1);
			c=strchr(def_section,']');
			if(c){
				*c=0;
				if(c[1]){
					msg="Garbage after ]";
					break;
				}
			}else{
				msg="Missing ]";
				break;
			}

		}else{
			byte *sect,*name,*value;

			name=c;
			while(*c && !Cspace(*c))
				c++;
			while(*c && Cspace(*c))
				*c++=0;
			value=c;

			if(!strcasecmp(name,"include")){
				if(!cf_subread(value,level+1)){
					msg="Included from here";
					break;
				}
			}else{
				c=strchr(name,'.');
				if(!c)
					sect=def_section;
				else{
					sect=name;
					*c++=0;
					name=c;
				}

				msg=cf_set_item(sect,name,value);
			}
			if(msg)
				break;
		}

	}	/* for every line */

	if(msg)
		log(L_ERROR,"%s, line %d: %s",filename,line,msg);
	bclose(b);
	return !msg;
}

void cf_read(byte *filename)
{
	if(!cf_subread(filename,0))
		die("Reading config file %s failed",filename);
	cfdeffile = NULL;
}

int cf_getopt(int argc,char * const argv[],
		const char *shortopts,const struct option *longopts,
		int *longindex)
{
	int res;

	do{
		res=getopt_long(argc,argv,shortopts,longopts,longindex);
		if(res=='S'){
			byte *sect,*name,*value;
			byte *c;
			byte *msg=NULL;

			name=optarg;
			c=strchr(name,'=');
			if(!c){
				msg="Missing argument";
				sect=value="";
			}else{
				*c++=0;
				value=c;

				c=strchr(name,'.');
				if(!c)
					sect="";
				else{
					sect=name;
					*c++=0;
					name=c;
				}

				if (cfdeffile)
					cf_read(cfdeffile);
				msg=cf_set_item(sect,name,value);
			}
			if(msg)
				die("Invalid command line argument %s.%s=%s: %s",sect,name,value,msg);

		}else if(res=='C'){
			cf_read(optarg);
		}else{
			/* unhandled option or end of options */
			if(cfdeffile)
				cf_read(cfdeffile);
			return res;
		}
	}while(1);
}
