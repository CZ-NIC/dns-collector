/* Test for configuration parser */

#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "lib/lib.h"
#include "lib/conf.h"

static int robert=14;
static int spalek=-3;
static char *heslo="prazdne";
static int nastaveni1=0,nastaveni2=1;

static byte *set_nastaveni(struct cfitem *item, byte *value)
{
	int id;
	if(!strcasecmp(value,"one"))
		id=1;
	else if(!strcasecmp(value,"two"))
		id=2;
	else if(!strcasecmp(value,"three"))
		id=3;
	else if(!strcasecmp(value,"four"))
		id=4;
	else
		return "Invalid value of nastaveni";
	if(!strcasecmp(item->name,"nastaveni1"))
		nastaveni1=id;
	else if(!strcasecmp(item->name,"nastaveni2"))
		nastaveni2=id;
	else
		return "Internal error of nastaveni";
	return NULL;
}

static struct cfitem jmeno[]={
	{"robert",	ct_int,	&robert},
	{"spalek",	ct_int,	&spalek},
	{"heslo",	ct_string,	&heslo},
	{"nastaveni1",	ct_function,	&set_nastaveni},
	{"nastaveni2",	ct_function,	&set_nastaveni},
	{NULL,		0,	NULL}
};

static int vek=22;
static int vyska=178;
static int vaha=66;

static struct cfitem telo[]={
	{"vek",		ct_int,	&vek},
	{"vyska",	ct_int,	&vyska},
	{"vaha",	ct_int,	&vaha},
	{NULL,		0,	NULL}
};

static byte shortopts[] = "abcp:q:r::";
static struct option longopts[] =
{
	{"ahoj",	0, 0, 'a'},
	{"bida",	0, 0, 'b'},
	{"citron",	0, 0, 'c'},
	{"pivo",	1, 0, 'p'},
	{"qwerty",	1, 0, 'q'},
	{"rada",	2, 0, 'r'},
	{NULL,		0, 0, 0}
};

int main(int argc, char *argv[])
{
	int c;

	cf_register("jmeno",jmeno);
	cf_register("telo",telo);
	cf_register_opts(shortopts,longopts);

	while(1){
		c=cf_getopt(argc,argv,NULL);
		if(c==-1)
			break;
		else switch(c){
			case 'a':
			case 'b':
			case 'c':
				printf("option %c\n",c);
				break;

			case 'p':
			case 'q':
				printf("option %c with parameter %s\n",c,optarg);
				break;
			case 'r':
				if(optarg)
					printf("option r with optional parameter %s\n",optarg);
				else
					printf("option r without optional parameter\n");
				break;
			case '?':
				//printf("invalid parameter %d: %s\n",optind,argv[optind]);
				break;
			case ':':
				//printf("missing parameter for %d: %s\n",optind,argv[optind]);
				break;
			default:
				printf("getopt is confused, it returns %c\n",c);
				break;
		}
	}

	if (optind < argc)
	{
		printf ("non-option ARGV-elements: ");
		while (optind < argc)
			printf ("%s ", argv[optind++]);
		printf ("\n");
	}

	printf("robert=%d, spalek=%d, heslo=%s, nastaveni1/2=%d/%d\n",
			robert,spalek,heslo,nastaveni1,nastaveni2);
	printf("vek=%d, vyska=%d, vaha=%d\n",
			vek,vyska,vaha);

	return 0;
}
