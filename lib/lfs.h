/*
 *	Sherlock Library -- Large File Support
 *
 *	(c) 1999--2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_LFS_H
#define _SHERLOCK_LFS_H

#include <fcntl.h>
#include <unistd.h>

#ifdef SHERLOCK_CONFIG_LFS

#define sh_open open64
#define sh_seek lseek64
#define sh_pread pread64
#define sh_pwrite pwrite64
#define sh_ftruncate ftruncate64
#define sh_mmap(a,l,p,f,d,o) mmap64(a,l,p,f,d,o)
#define sh_pread pread64
#define sh_pwrite pwrite64

#else	/* !SHERLOCK_CONFIG_LFS */

#define sh_open open
#define sh_seek(f,o,w) lseek(f,o,w)
#define sh_ftruncate(f,o) ftruncate(f,o)
#define sh_mmap(a,l,p,f,d,o) mmap(a,l,p,f,d,o)
#define sh_pread pread
#define sh_pwrite pwrite

#endif	/* !SHERLOCK_CONFIG_LFS */

#define SHERLOCK_HAVE_PREAD

#endif	/* !_SHERLOCK_LFS_H */
