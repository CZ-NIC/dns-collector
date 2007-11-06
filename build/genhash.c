/*
 *	Generator of Word Recognition Hash Tables
 *	(a.k.a. simple gperf replacement)
 *
 *	(c) 1999 Martin Mares <mj@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

struct word {
  struct word *next;
  char *w;
  char *extra;
};

static unsigned int hash(char *c)
{
  unsigned int h = 0;
  while (*c)
    h = (h * 37) + *c++;
  return h;
}

static int				/* Sequential search */
fast_isprime(unsigned x)		/* We know x != 2 && x != 3 */
{
  unsigned test = 5;

  for(;;)
    {
      if (!(x % test))
	return 0;
      if (x / test <= test)
	return 1;
      test += 2;			/* 6k+1 */
      if (!(x % test))
	return 0;
      if (x / test <= test)
	return 1;
      test += 4;			/* 6k-1 */
    }
}

static unsigned int
nextprime(unsigned x)			/* Returns some prime greater than X */
{
  if (x <= 5)
    return 5;
  x += 5 - (x % 6);			/* x is 6k-1 */
  for(;;)
    {
      if (fast_isprime(x))
	return x;
      x += 2;				/* 6k+1 */
      if (fast_isprime(x))
	return x;
      x += 4;				/* 6k-1 */
    }
}

int main(int argc, char **argv)
{
  FILE *fi, *fo;
  struct word *words = NULL;
  struct word *w, **ht;
  char buf[1024], *c, namebuf[256];
  int cnt = 0;
  int skip, i, size;

  if (argc != 4)
    {
      fprintf(stderr, "Usage: genhash <input> <output> <func_name>\n");
      return 1;
    }
  fi = fopen(argv[1], "r");
  if (!fi) { fprintf(stderr, "Cannot open input file: %m\n"); return 1; }
  fo = fopen(argv[2], "w");
  if (!fo) { fprintf(stderr, "Cannot open output file: %m\n"); return 1; }

  buf[0] = 0;
  fgets(buf, sizeof(buf)-1, fi);
  if (strncmp(buf, "%{", 2)) { fprintf(stderr, "Syntax error at <%s>\n", buf); return 1; }
  fputs(buf+2, fo);
  while (fgets(buf, sizeof(buf)-1, fi) && strcmp(buf, "%}\n"))
    fputs(buf, fo);
  fgets(namebuf, sizeof(namebuf)-1, fi);
  if (strncmp(namebuf, "struct ", 7) || !(c = strchr(namebuf+7, ' ')))
    { fprintf(stderr, "Syntax error at <%s>\n", namebuf); return 1; }
  *c = 0;
  while (fgets(buf, sizeof(buf)-1, fi) && strcmp(buf, "%%\n"))
    ;
  while (fgets(buf, sizeof(buf)-1, fi))
    {
      c = strchr(buf, '\n');
      if (c)
	*c = 0;
      c = strchr(buf, ',');
      w = alloca(sizeof(struct word));
      if (c)
	*c++ = 0;
      else
	{ fprintf(stderr, "No comma?\n"); return 1; }
      w->w = alloca(strlen(buf)+1);
      strcpy(w->w, buf);
      w->extra = alloca(strlen(c)+1);
      strcpy(w->extra, c);
      w->next = words;
      words = w;
      cnt++;
    }
  cnt = cnt*12/10;
  size = 16;
  while (size < cnt)
    size += size;
  skip = nextprime(size*3/4);

  ht = alloca(size * sizeof(struct word *));
  bzero(ht, size * sizeof(struct word *));
  for(w=words; w; w=w->next)
    {
      int h = hash(w->w) & (size - 1);
      while (ht[h])
	h = (h + skip) & (size - 1);
      ht[h] = w;
    }

  fprintf(fo, "static %s htable[] = {\n", namebuf);
  for(i=0; i<size; i++)
    if (ht[i])
      fprintf(fo, "{ \"%s\", %s },\n", ht[i]->w, ht[i]->extra);
    else
      fprintf(fo, "{ NULL },\n");
  fprintf(fo, "};\n\nconst %s *%s(const char *x, unsigned int len)\n\
{\n\
  const char *c = x;\n\
  unsigned int h = 0;\n\
  while (*c)\n\
    h = (h * 37) + *c++;\n\
  h = h & %d;\n\
  while (htable[h].name)\n\
    {\n\
      if (!strcmp(htable[h].name, x))\n\
        return &htable[h];\n\
      h = (h + %d) & %d;\n\
    }\n\
  return NULL;\n\
}\n", namebuf, argv[3], size-1, skip, size-1);

  return 0;
}
