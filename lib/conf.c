/*
 *	Sherlock Library -- Reading configuration files
 *
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>

#include "lib/chartype.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"

#include "lib/conf.h"

#define	BUFFER		1024
#define	MAX_LEVEL	8

static struct cfitem *cfsection;

void cf_register(struct cfitem *items)
{
	if(items[0].type!=CT_SECTION)
		die("Invalid configuration section, first item must be of type CT_SECTION");
	items[0].var=cfsection;
	cfsection=items;
}

byte *cf_set_item(byte *sect, byte *name, byte *value)
{
	struct cfitem *item;
	byte *msg=NULL;

	item=cfsection;
	while(item && strcasecmp(item->name,sect))
		item=item->var;
	if(!item)	/* ignore unknown section */
		return NULL;

	for(item++; item->type && strcasecmp(item->name,name); item++);

	switch(item->type){
		case CT_INT:
			{
				char *end;
				*((uns *) item->var) = strtoul(value, &end, 0);
				if (end && *end)
					msg = "Invalid number";
				break;
			}
		case CT_STRING:
			*((byte **) item->var) = stralloc(value);
			break;
		case CT_FUNCTION:
			msg = ((ci_func) item->var)(item, value);
			break;
		default:
			msg = "Unknown keyword";
			break;
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
		
	fd=sh_open(filename,O_RDONLY, 0666);
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

		c=buf;
		while(*c && Cspace(*c))
			c++;
		if(!*c || *c=='#')
			continue;

		if(*c=='['){
			strcpy(def_section,c+1);
			c=strchr(def_section,']');
			if(c)
				*c=0;
			else{
				msg="Missing ]";
				break;
			}

		}else if(*c=='<'){
			if(!cf_subread(c+1,level+1)){
				msg="";
				break;
			}
		
		}else{
			byte *sect,*name,*value;

			name=c;
			c=strpbrk(c," \t");
			while(c && *c && Cspace(*c))
				*c++=0;
			if(!c || !*c){
				msg="Missing argument";
				break;
			}
			value=c;

			c=strchr(name,'.');
			if(!c)
				sect=def_section;
			else{
				sect=name;
				*c++=0;
				name=c;
			}

			msg=cf_set_item(sect,name,value);
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

				msg=cf_set_item(sect,name,value);
			}
			if(msg)
				die("Invalid command line argument %s.%s=%s: %s",sect,name,value,msg);

		}else if(res=='C'){
			cf_read(optarg);
		}else{	/* unhandled option */
			return res;
		}
	}while(1);
}

