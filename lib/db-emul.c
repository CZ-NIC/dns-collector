/*
 *	Sherlock Library -- SDBM emulator at top of GDBM
 *
 *	(c) 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/lib.h"
#include "lib/db.h"

#include <gdbm.h>

struct sdbm {
  GDBM_FILE db;
  datum prevkey;
};

struct sdbm *
sdbm_open(struct sdbm_options *o)
{
  struct sdbm *d = xmalloc(sizeof(struct sdbm));
  d->db = gdbm_open(o->name,
		    (o->page_order ? (1 << o->page_order) : 0),
		    ((o->flags & SDBM_WRITE) ? ((o->flags & SDBM_CREAT) ? GDBM_WRCREAT : GDBM_WRITER) : GDBM_READER)
		      | ((o->flags & SDBM_SYNC) ? GDBM_SYNC : 0),
		    0666,
		    NULL);
  if (o->cache_size)
    gdbm_setopt(d->db, GDBM_CACHESIZE, &o->cache_size, sizeof(o->cache_size));
  d->prevkey.dptr = NULL;
  return d;
}

void
sdbm_close(struct sdbm *d)
{
  sdbm_rewind(d);
  gdbm_close(d->db);
  free(d);
}

static int
sdbm_put_user(byte *D, uns Dl, byte *val, uns *vallen)
{
  if (vallen)
    {
      if (*vallen < Dl)
	return 1;
      *vallen = Dl;
    }
  if (val)
    memcpy(val, D, Dl);
  return 0;
}

int
sdbm_store(struct sdbm *d, byte *key, uns keylen, byte *val, uns vallen)
{
  datum K, V;
  int rc;

  K.dptr = key;
  K.dsize = keylen;
  V.dptr = val;
  V.dsize = vallen;
  rc = gdbm_store(d->db, K, V, GDBM_INSERT);
  return (rc < 0) ? rc : !rc;
}

int
sdbm_replace(struct sdbm *d, byte *key, uns keylen, byte *val, uns vallen)
{
  datum K, V;
  int rc;

  if (!val)
    return sdbm_delete(d, key, keylen);
  K.dptr = key;
  K.dsize = keylen;
  V.dptr = val;
  V.dsize = vallen;
  rc = gdbm_store(d->db, K, V, GDBM_REPLACE);
  return (rc < 0) ? rc : !rc;
}

int
sdbm_delete(struct sdbm *d, byte *key, uns keylen)
{
  datum K;

  K.dptr = key;
  K.dsize = keylen;
  return !gdbm_delete(d->db, K);
}

int
sdbm_fetch(struct sdbm *d, byte *key, uns keylen, byte *val, uns *vallen)
{
  datum K, V;
  int rc;

  K.dptr = key;
  K.dsize = keylen;
  if (!val && !vallen)
    return gdbm_exists(d->db, K);
  V = gdbm_fetch(d->db, K);
  if (!V.dptr)
    return 0;
  rc = sdbm_put_user(V.dptr, V.dsize, val, vallen);
  free(V.dptr);
  return rc ? SDBM_ERROR_TOO_LARGE : 1;
}

void
sdbm_rewind(struct sdbm *d)
{
  if (d->prevkey.dptr)
    {
      free(d->prevkey.dptr);
      d->prevkey.dptr = NULL;
    }
}

int
sdbm_get_next(struct sdbm *d, byte *key, uns *keylen, byte *val, uns *vallen)
{
  datum K;

  if (d->prevkey.dptr)
    {
      K = gdbm_nextkey(d->db, d->prevkey);
      free(d->prevkey.dptr);
    }
  else
    K = gdbm_firstkey(d->db);
  d->prevkey = K;
  if (!K.dptr)
    return 0;
  if (sdbm_put_user(K.dptr, K.dsize, key, keylen))
    return SDBM_ERROR_TOO_LARGE;
  if (val || vallen)
    return sdbm_fetch(d, key, *keylen, val, vallen);
  return 1;
}

void
sdbm_sync(struct sdbm *d)
{
}
