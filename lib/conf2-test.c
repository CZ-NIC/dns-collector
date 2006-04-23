/*
 *	Insane tester of reading configuration files
 *
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/conf2.h"
#include "lib/clists.h"
#include "lib/fastbuf.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>

static int verbose;

struct sub_sect_1 {
  struct cnode n;
  byte *name;
  byte *level;
  int confidence[2];
  double *list;
};

static struct sub_sect_1 sec1 = { {}, "Charlie", "WBAFC", { 0, -1}, DARY_ALLOC(double, 3, 1e4, -1e-4, 8) };

static byte *
init_sec_1(struct sub_sect_1 *s)
{
  if (s == &sec1) {			// this is a static variable; skip clearing
    DARY_LEN(sec1.list) = 3;		// XXX: fix for the bug in DARY_ALLOC()
    return NULL;
  }
  s->name = "unknown";
  s->level = "default";
  s->confidence[0] = 5;
  s->confidence[1] = 6;
  // leave s->list==NULL
  return NULL;
}

static byte *
commit_sec_1(struct sub_sect_1 *s)
{
  if (s->confidence[0] < 0 || s->confidence[0] > 10)
    return "Well, this can't be";
  return NULL;
}

static struct cf_section cf_sec_1 = {
  CF_TYPE(struct sub_sect_1),
  CF_INIT(init_sec_1),
  CF_COMMIT(commit_sec_1),
#define F(x)	PTR_TO(struct sub_sect_1, x)
  CF_ITEMS {
    CF_STRING("name", F(name)),
    CF_STRING("level", F(level)),
    CF_INT_ARY("confidence", F(confidence[0]), 2),		// XXX: the [0] is needed for the sake of type checking
    CF_DOUBLE_DYN("list", F(list), 100),
    CF_END
  }
#undef F
};

static uns nr1 = 15;
static int *nrs1 = DARY_ALLOC(int, 5, 5, 4, 3, 2, 1);
static int nrs2[5];
static byte *str1 = "no worries";
static byte **str2 = DARY_ALLOC(byte *, 2, "Alice", "Bob");
static u64 u1 = 0xCafeBeefDeadC00ll;
static double d1 = -1.1;
static struct clist secs;
static time_t t1, t2;
static u32 ip;

static byte *
init_top(void *ptr UNUSED)
{
  for (uns i=0; i<5; i++)
  {
    struct sub_sect_1 *s = xmalloc(sizeof(struct sub_sect_1));	// XXX: cannot by cf_malloc(), because it's deleted when cf_reload()'ed
    cf_init_section("slaves", &cf_sec_1, s, 1);
    s->confidence[1] = i;
    clist_add_tail(&secs, &s->n);
  }
  return NULL;
}

static byte *
commit_top(void *ptr UNUSED)
{
  if (nr1 != 15)
    return "Don't touch my variable!";
  return NULL;
}

static byte *
time_parser(uns number, byte **pars, time_t *ptr)
{
  *ptr = number ? atoi(pars[0]) : time(NULL);
  return NULL;
}

static struct cf_section cf_top = {
  CF_INIT(init_top),
  CF_COMMIT(commit_top),
  CF_ITEMS {
    CF_UNS("nr1", &nr1),
    CF_INT_DYN("nrs1", &nrs1, 1000),
    CF_INT_ARY("nrs2", nrs2, 5),
    CF_STRING("str1", &str1),
    CF_STRING_DYN("str2", &str2, 20),
    CF_U64("u1", &u1),
    CF_DOUBLE("d1", &d1),
    CF_PARSER("FirstTime", &t1, time_parser, -1),
    CF_PARSER("SecondTime", &t2, time_parser, 1),
    CF_SECTION("master", &sec1, &cf_sec_1),
    CF_LIST("slaves", &secs, &cf_sec_1),
    CF_IP("ip", &ip),
    CF_END
  }
};

static byte short_opts[] = CF_SHORT_OPTS "v";
static struct option long_opts[] = {
	CF_LONG_OPTS
	{"verbose",	0, 0, 'v'},
	{NULL,		0, 0, 0}
};

static char *help = "\
Usage: conf2-test <options>\n\
\n\
Options:\n"
CF_USAGE
"-v\t\t\tBe verbose\n\
";

static void NONRET
usage(void)
{
	fputs(help, stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
  log_init(argv[0]);
  cf_declare_section("top", &cf_top, 0);
  cf_def_file = "lib/conf2.t";

  int opt;
  while ((opt = cf_get_opt(argc, argv, short_opts, long_opts, NULL)) >= 0)
    switch (opt) {
      case 'v': verbose++; break;
      default: usage();
    }
  if (optind < argc)
    usage();

  //cf_reload("non-existent file");

  struct fastbuf *out = bfdopen(1, 1<<14);
  cf_dump_sections(out);
  bclose(out);

  return 0;
}
