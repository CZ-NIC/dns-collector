/*
 *	Sherlock Library -- Database Manager -- Simple Tests
 *
 *	(c) 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#if 1
#include "db.c"
#else
#include "db-emul.c"
#endif

int main(void)
{
  struct sdbm *d;
  struct sdbm_options o = {
    name: "db.test",
    flags: SDBM_CREAT | SDBM_WRITE,
    page_order: 10,
    cache_size: 1024,
    key_size: -1,
    val_size: 4
  };
  byte buf[256];
  int i, j, k, l, m, n;
#define BIGN 10000

  puts("OPEN");
  d = sdbm_open(&o);
  if (!d)
    die("failed: %m");

  puts("WRITE");
  for(i=0; i<BIGN; i++)
    {
      sprintf(buf, "%d", i);
      k = sdbm_store(d, buf, strlen(buf), (byte *) &i, sizeof(i));
//      printf("%s:%d\r", buf, k);
      fflush(stdout);
    }
  sdbm_sync(d);

  puts("REWRITE");
  for(i=0; i<BIGN; i++)
    {
      sprintf(buf, "%d", i);
      k = sdbm_replace(d, buf, strlen(buf), (byte *) &i, sizeof(i));
//      printf("%s:%d\r", buf, k);
      if (!k) { printf("XXX %s %d\n", buf, k); return 1; }
      fflush(stdout);
    }
  sdbm_sync(d);

  puts("READ");
  for(i=0; i<BIGN; i++)
    {
      sprintf(buf, "%d", i);
      j = sdbm_fetch(d, buf, strlen(buf), NULL, NULL);
//      printf("%s:%d\r", buf, j);
      fflush(stdout);
      if (!j)
	{ printf("\nERR: %s %d\n", buf, j); return 1; }
    }

  puts("FETCH");
  sdbm_rewind(d);
  l = 0;
  m = 0;
  n = 0;
  for(;;)
    {
      j = sizeof(buf);
      i = sdbm_get_next(d, buf, &j, (byte *) &k, NULL);
      if (i < 0) { puts("ERRRR\n"); return 1; }
      if (!i) break;
      buf[j] = 0;
//      printf("%s %d\n", buf, k);
      l += k;
      m += n++;
    }
  if (n != BIGN) { printf("MISMATCH COUNT %d\n", n); return 1; }
  if (l != m) { printf("MISMATCH %d != %d\n", l, m); return 1; }

  puts("DELETE");
  for(i=0; i<BIGN; i++)
    {
      sprintf(buf, "%d", i);
      j = sdbm_delete(d, buf, strlen(buf));
//      printf("%s:%d\r", buf, j);
      fflush(stdout);
      if (!j)
	{ printf("\nERR: %s %d\n", buf, j); return 1; }
    }
  sdbm_sync(d);

  puts("CHECK");
  sdbm_rewind(d);
  if (sdbm_get_next(d, buf, NULL, NULL, NULL)) { puts("NOT EMPTY!"); return 1; }

  puts("CLOSE");
  sdbm_close(d);
  puts("DONE");
  return 0;
}
