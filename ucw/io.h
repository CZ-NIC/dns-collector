/*
 *	UCW Library -- Large File Support
 *
 *	(c) 1999--2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_IO_H
#define _UCW_IO_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define careful_read ucw_careful_read
#define careful_write ucw_careful_write
#define mmap_file ucw_mmap_file
#define munmap_file ucw_munmap_file
#define sync_dir ucw_sync_dir
#endif

#ifdef CONFIG_UCW_LARGE_FILES

#define ucw_open open64
#define ucw_seek lseek64
#define ucw_pread pread64
#define ucw_pwrite pwrite64
#define ucw_ftruncate ftruncate64
#define ucw_mmap(a,l,p,f,d,o) mmap64(a,l,p,f,d,o)
#define ucw_pread pread64
#define ucw_pwrite pwrite64
#define ucw_stat stat64
#define ucw_fstat fstat64
typedef struct stat64 ucw_stat_t;

#else	/* !CONFIG_UCW_LARGE_FILES */

#define ucw_open open
#define ucw_seek(f,o,w) lseek(f,o,w)
#define ucw_ftruncate(f,o) ftruncate(f,o)
#define ucw_mmap(a,l,p,f,d,o) mmap(a,l,p,f,d,o)
#define ucw_pread pread
#define ucw_pwrite pwrite
#define ucw_stat stat
#define ucw_fstat fstat
typedef struct stat ucw_stat_t;

#endif	/* !CONFIG_UCW_LARGE_FILES */

#if defined(_POSIX_SYNCHRONIZED_IO) && (_POSIX_SYNCHRONIZED_IO > 0)
#define ucw_fdatasync fdatasync
#else
#define ucw_fdatasync fsync
#endif

#define HAVE_PREAD

/* io-size.c */

ucw_off_t ucw_file_size(const char *name);

/* io-mmap.c */

void *mmap_file(const char *name, unsigned *len, int writeable);
void munmap_file(void *start, unsigned len);

/* io-careful.c */

int careful_read(int fd, void *buf, size_t len);
int careful_write(int fd, const void *buf, size_t len);

/* io-sync.c */

void sync_dir(const char *name);

#endif	/* !_UCW_IO_H */
