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

/*
 *  We'd like to use pread/pwrite for our file accesses, but unfortunately it
 *  isn't simple at all since all libc's until glibc 2.1 don't define it.
 */

#ifdef __linux__
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ > 0
/* glibc 2.1 or newer -> pread/pwrite supported automatically */
#define SHERLOCK_HAVE_PREAD
#elif defined(i386) && defined(__GLIBC__)
/* glibc 2.0 on i386 -> call syscalls directly */
#include <asm/unistd.h>
#include <syscall-list.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef SYS_pread
#define SYS_pread 180
#endif
static int pread(unsigned int fd, void *buf, size_t size, loff_t where)
{ return syscall(SYS_pread, fd, buf, size, where); }
#ifndef SYS_pwrite
#define SYS_pwrite 181
#endif
static int pwrite(unsigned int fd, void *buf, size_t size, loff_t where)
{ return syscall(SYS_pwrite, fd, buf, size, where); }
#define SHERLOCK_HAVE_PREAD
#elif defined(i386)
/* old libc on i386 -> call syscalls directly the old way */
#include <asm/unistd.h>
static _syscall4(int, pread, unsigned int, fd, void *, buf, size_t, size, loff_t, where);
static _syscall4(int, pwrite, unsigned int, fd, void *, buf, size_t, size, loff_t, where);
#define SHERLOCK_HAVE_PREAD
#endif
#endif	/* __linux__ */

#endif	/* !_SHERLOCK_LFS_H */
