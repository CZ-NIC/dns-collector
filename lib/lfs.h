/*
 *	Sherlock Library -- Large File Support
 *
 *	(c) 1999--2001 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_LFS_H
#define _SHERLOCK_LFS_H

#include <fcntl.h>
#include <unistd.h>

#ifdef SHERLOCK_CONFIG_LFS

#ifdef SHERLOCK_CONFIG_LFS_LIBC

/*
 *  Unfortunately, we need to configure this manually since
 *  out-of-the-box glibc 2.1 offers the 64-bit calls, but
 *  converts them to 32-bit syscalls. Damn it!
 */

#define sh_open open64
#define sh_seek lseek64
#define sh_pread pread64
#define sh_pwrite pwrite64
#define sh_ftruncate ftruncate64
#define sh_mmap(a,l,p,f,d,o) mmap64(a,l,p,f,d,o)
#define SHERLOCK_HAVE_PREAD

#else

#error Non-libc interface to LFS is currently broken, you have to fix it.

/*
 *  Talk directly with the kernel. The implementations of LFS in Linux 2.2
 *  and 2.4 differ, but fortunately for us only in things like stat64 which
 *  we don't need to use.
 */

#ifndef O_LARGEFILE
#if defined(__linux__) && defined(__i386__)
#define O_LARGEFILE 0100000
#else
#error O_LARGEFILE unknown
#endif
#endif

static inline int
sh_open(char *name, int flags, int mode)
{
  return open(name, flags | O_LARGEFILE, mode);
}

#if 0

/* A "do it yourself" solution */

#include <asm/unistd.h>
#include <errno.h>

_syscall5(int, _llseek, int, fd, int, hi, int, lo, loff_t *, result, int, whence);

static inline loff_t sh_seek(int fd, sh_off_t pos, int whence)
{
  loff_t result;
  int err;

  err = _llseek(fd, pos >> 32, pos, &result, whence);
  return (err < 0) ? err : result;
}
#else

#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ > 0
/* glibc 2.1 or newer -> has lseek64 */
#define sh_seek(f,o,w) lseek64(f,o,w)
#else
/* Touching hidden places in glibc */
extern loff_t llseek(int fd, loff_t pos, int whence);
#define sh_seek(f,o,w) llseek(f,o,w)
#endif

#endif

#endif  /* !SHERLOCK_CONFIG_LFS_LIBC */

#else	/* !SHERLOCK_CONFIG_LFS */

#define sh_open open
#define sh_seek(f,o,w) lseek(f,o,w)
#define sh_ftruncate(f,o) ftruncate(f,o)
#define sh_mmap(a,l,p,f,d,o) mmap(a,l,p,f,d,o)

#endif	/* !SHERLOCK_CONFIG_LFS */

/*
 *  We'd like to use pread/pwrite for our file accesses, but unfortunately it
 *  isn't simple at all since all libc's until glibc 2.1 don't define it.
 */

#ifndef SHERLOCK_HAVE_PREAD

#ifdef __linux__
#define SHERLOCK_HAVE_PREAD
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ > 0
/* glibc 2.1 or newer -> pread/pwrite supported automatically */
#ifdef SHERLOCK_CONFIG_LFS
/* but have to use the 64-bit versions explicitly */
#define sh_pread pread64
#define sh_pwrite pwrite64
#else
#define sh_pread pread
#define sh_pwrite pwrite
#endif
#elif defined(i386) && defined(__GLIBC__)
/* glibc 2.0 on i386 -> call syscalls directly */
#include <asm/unistd.h>
#include <syscall-list.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef SYS_pread
#define SYS_pread 180
#endif
static int sh_pread(unsigned int fd, void *buf, size_t size, loff_t where)
{ return syscall(SYS_pread, fd, buf, size, where); }
#ifndef SYS_pwrite
#define SYS_pwrite 181
#endif
static int sh_pwrite(unsigned int fd, void *buf, size_t size, loff_t where)
{ return syscall(SYS_pwrite, fd, buf, size, where); }
#elif defined(i386)
/* old libc on i386 -> call syscalls directly the old way */
#include <asm/unistd.h>
#include <errno.h>
static _syscall4(int, pread, unsigned int, fd, void *, buf, size_t, size, loff_t, where);
static _syscall4(int, pwrite, unsigned int, fd, void *, buf, size_t, size, loff_t, where);
#define sh_pread pread
#define sh_pwrite pwrite
#else
#undef SHERLOCK_HAVE_PREAD
#endif
#endif	/* __linux__ */

#endif  /* !SHERLOCK_HAVE_PREAD */

#endif	/* !_SHERLOCK_LFS_H */
