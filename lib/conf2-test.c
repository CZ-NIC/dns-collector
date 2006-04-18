/*
 *	Insane tester of reading configuration files
 *
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/conf2.h"
#include "lib/clists.h"

#include <stdlib.h>
#include <time.h>

struct sub_sect_1 {
  byte *name;
  byte *level;
  int confidence;
};

static byte *
init_sec_1(void *ptr)
{
  struct sub_sect_1 *s = ptr;
  s->name = "unknown";
  s->level = "default";
  s->confidence = 5;
  return NULL;
}

static byte *
commit_sec_1(void *ptr)
{
  struct sub_sect_1 *s = ptr;
  if (s->confidence < 0 || s->confidence > 10)
    return "Well, this can't be";
  return NULL;
}

static struct cf_section cf_sec_1 = {
  .size = sizeof(struct sub_sect_1),
  .init = init_sec_1,
  .commit = commit_sec_1,
  .cfg = (struct cf_item[]) {
#define F(x)	CF_FIELD(struct sub_sect_1, x)
    CF_STRING("name", F(name)),
    CF_STRING("level", F(level)),
    CF_INT("confidence", F(confidence)),
    CF_END
#undef F
  }
};

static int nr1 = 15;
static int *nrs1 = ARRAY_ALLOC(int, 5, 5, 4, 3, 2, 1);
static int *nrs2;
static byte *str1 = "no worries";
static byte **str2 = ARRAY_ALLOC(byte *, 2, "Alice", "Bob");
static u64 u1 = 0xCafeBeefDeadC00ll;
static double d1 = -1.1;
static struct sub_sect_1 sec1 = { "Charlie", "WBAFC", 0 };
static struct clist secs;
static time_t t1, t2;

static byte *
commit_top(void *ptr UNUSED)
{
  if (nr1 != 15)
    return "Don't touch my variable!";
  return NULL;
}

static byte *
time_parser(byte *name UNUSED, uns number, byte **pars, void *ptr)
{
  * (time_t*) ptr = number ? atoi(pars[0]) : time(NULL);
  return NULL;
}

static struct cf_section cf_top = {
  .commit = commit_top,
  .cfg = (struct cf_item []) {
    CF_INT("nr1", &nr1),
    CF_INT_AR("nrs1", &nrs1, 5),
    CF_INT_AR("nrs2", &nrs2, -1000),
    CF_STRING("str1", &str1),
    CF_STRING_AR("str2", &str2, 2),
    CF_U64("u1", &u1),
    CF_DOUBLE("d1", &d1),
    CF_PARSER("FirstTime", &t1, time_parser, -1),
    CF_PARSER("SecondTime", &t2, time_parser, 1),
    CF_SUB_SECTION("master", &sec1, &cf_sec_1),
    CF_LINK_LIST("slaves", &secs, &cf_sec_1),
    CF_END
  }
};

int
main(void)
{
  return 0;
}
