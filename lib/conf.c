/* Reading conf files
 * Robert Spalek, (c) 2001, robert@ucw.cz
 * $Id: conf.c,v 1.1 2001/01/07 21:21:53 robert Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include "lib/conf.h"

#define	BUFFER		1024
#define	MAX_LEVEL	8

#define	MAX_SECTIONS	64

static struct {
	byte *section;
	struct cfitem *items;
} cfsection[MAX_SECTIONS];
static int cfsections;

#define	MAX_SHORT_OPTS	128
#define	MAX_LONG_OPTS	64

static byte shortopts[MAX_SHORT_OPTS] = "S:C:";
static int shortlen=4;

static struct option longopts[MAX_LONG_OPTS] =
{
	{"set",		1, 0, 'S'},
	{"config",	1, 0, 'C'}
};
static int longlen=2;

void cf_register(byte *section,struct cfitem *items)
{
	if(cfsections>=MAX_SECTIONS)
		die("too many modules %d",cfsections);
	cfsection[cfsections].section=section;
	cfsection[cfsections].items=items;
	cfsections++;
}

static byte *cf_set_item(byte *sect, byte *name, byte *value)
{
	int idsect;
	struct cfitem *item;
	byte *msg=NULL;

	idsect=0;
	while(idsect<cfsections && strcasecmp(sect,cfsection[idsect].section))
		idsect++;
	if(idsect>=cfsections)	/* ignore unknown section */
		return NULL;

	item=cfsection[idsect].items;
	while(item->type && strcasecmp(name,item->name))
		item++;
	switch(item->type){
		case ct_int:
			{
				char *end;
				*((uns *) item->var) = strtoul(value, &end, 0);
				if (end && *end)
					msg = "Invalid number";
				break;
			}
		case ct_string:
			*((byte **) item->var) = stralloc(value);
			break;
		case ct_function:
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
	struct fastbuf *b;
	byte def_section[BUFFER];
	int line;
	byte *msg=NULL;

	if(level>=MAX_LEVEL){
		log("Too many nested files %d",level);
		return 0;
	}
		
	b=bopen(filename,O_RDONLY,4096);
	if(!b){
		log("Cannot open file %s",filename);
		return 0;
	}

	def_section[0]=0;
	line=0;
	while(1){
		byte buf[BUFFER];
		byte *c;

		if(!bgets(b,buf,BUFFER))
			break;

		c=buf;
		while(*c && isspace(*c))
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

		}else if(*c='<'){
			if(!cf_subread(c+1,level+1)){
				msg="";
				break;
			}
		
		}else{
			byte *sect,*name,*value;

			name=c;
			c=strpbrk(c," \t");
			while(c && *c && isspace(*c))
				c++;
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
		log("%s, line %d: %s",msg);
	bclose(b);
	return !msg;
}

int cf_read(byte *filename)
{
	return cf_subread(filename,0);
}

void cf_read_err(byte *filename)
{
	if(!cf_read(filename))
		die("Reading config file %s failed",filename);
}

void cf_register_opts(byte *so,struct option *lo)
{
	int l;

	l=strlen(so);
	if(shortlen+l>=MAX_SHORT_OPTS)
		die("Too many short options %d",shortlen+l);
	strcat(shortopts,so);

	l=longlen;
	while(lo->name){
		if(l>=MAX_LONG_OPTS)
			die("Too many long options %d",l);
		longopts[l++]=*lo++;
	}
}

int cf_getopt(int argc,char * const argv[], int *longindex)
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
			cf_read_err(optarg);

		}else{	/* unhandled option */
			return res;
		}

	}while(1);
}

