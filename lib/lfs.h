/*
 *	Sherlock Library -- Large File Support
 *
 *	(c) 1999 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef _SHERLOCK_LFS_H
#define _SHERLOCK_LFS_H

#ifdef SHERLOCK_CONFIG_LFS

#ifndef O_LARGEFILE
#if defined(__linux__) && defined(__i386__)
#define O_LARGEFILE 0100000
#else
#error O_LARGEFILE unknown
#endif
#endif

#define SHERLOCK_O_LARGEFILE O_LARGEFILE

#if 0

/* A "do it yourself" solution */

#include <asm/unistd.h>
#include <errno.h>

_syscall5(int, _llseek, int, fd, int, hi, int, lo, loff_t *, result, int, whence);

extern inline loff_t sh_seek(int fd, sh_off_t pos, int whence)
{
  loff_t result;
  int err;

  err = _llseek(fd, pos >> 32, pos, &result, whence);
  return (err < 0) ? err : result;
}
#else

/* Touching hidden places in glibc */
extern loff_t llseek(int fd, loff_t pos, int whence);
#define sh_seek(f,o,w) llseek(f,o,w)

#endif

#else	/* !SHERLOCK_CONFIG_LFS */

#define sh_seek(f,o,w) lseek(f,o,w)
#define SHERLOCK_O_LARGEFILE 0

#endif	/* !SHERLOCK_CONFIG_LFS */
#endif	/* !_SHERLOCK_LFS_H */
