/*
 *	UCW Library -- Large File Support
 *
 *	(c) 1999--2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_LFS_H
#define _UCW_LFS_H

#include <fcntl.h>
#include <unistd.h>

#ifdef CONFIG_LARGE_FILES

#define sh_open open64
#define sh_seek lseek64
#define sh_pread pread64
#define sh_pwrite pwrite64
#define sh_ftruncate ftruncate64
#define sh_mmap(a,l,p,f,d,o) mmap64(a,l,p,f,d,o)
#define sh_pread pread64
#define sh_pwrite pwrite64

#else	/* !CONFIG_LARGE_FILES */

#define sh_open open
#define sh_seek(f,o,w) lseek(f,o,w)
#define sh_ftruncate(f,o) ftruncate(f,o)
#define sh_mmap(a,l,p,f,d,o) mmap(a,l,p,f,d,o)
#define sh_pread pread
#define sh_pwrite pwrite

#endif	/* !CONFIG_LARGE_FILES */

#define HAVE_PREAD

static inline sh_off_t
sh_file_size(byte *name)
{
  int fd = sh_open(name, O_RDONLY);
  if (fd < 0)
    die("Cannot open %s: %m", name);
  sh_off_t len = sh_seek(fd, 0, SEEK_END);
  close(fd);
  return len;
}

#endif	/* !_UCW_LFS_H */
