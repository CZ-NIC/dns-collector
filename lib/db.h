/*
 *	Sherlock Library -- Fast Database Management Routines
 *
 *	(c) 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef _SHERLOCK_DB_H
#define _SHERLOCK_DB_H

struct sdbm;

struct sdbm_options {			/* Set to 0 for default */
  char *name;				/* File name */
  uns flags;				/* See SDBM_xxx below */
  uns page_order;			/* Binary logarithm of file page size */
  uns cache_size;			/* Number of cached pages */
  int key_size;				/* Key size, -1=variable */
  int val_size;				/* Value size, -1=variable */
};

struct sdbm_buf {
  uns size;
  byte data[0];
};

struct sdbm *sdbm_open(struct sdbm_options *);
void sdbm_close(struct sdbm *);
int sdbm_store(struct sdbm *, byte *key, uns keylen, byte *val, uns vallen);
int sdbm_replace(struct sdbm *, byte *key, uns keylen, byte *val, uns vallen); /* val == NULL -> delete */
int sdbm_delete(struct sdbm *, byte *key, uns keylen);
int sdbm_fetch(struct sdbm *, byte *key, uns keylen, byte *val, uns *vallen);		/* val can be NULL */
void sdbm_rewind(struct sdbm *);
int sdbm_get_next(struct sdbm *, byte *key, uns *keylen, byte *val, uns *vallen);	/* val can be NULL */
void sdbm_sync(struct sdbm *);

#define SDBM_CREAT		1	/* Create the database if it doesn't exist */
#define SDBM_WRITE		2	/* Open the database in read/write mode */
#define SDBM_SYNC		4	/* Sync after each operation */
#define SDBM_FAST		8	/* Don't sync on directory splits -- results in slightly faster
					 * operation, but reconstruction of database after program crash
					 * may be impossible.
					 */
#define SDBM_FSYNC		16	/* When syncing, call fsync() */

#define SDBM_ERROR_BAD_KEY_SIZE -1	/* Fixed key size doesn't match */
#define SDBM_ERROR_BAD_VAL_SIZE -2	/* Fixed value size doesn't match */
#define SDBM_ERROR_TOO_LARGE	-3	/* Key/value doesn't fit in buffer supplied */
#define SDBM_ERROR_READ_ONLY	-4	/* Database has been opened read only */

#endif
