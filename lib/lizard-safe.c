/*
 *	LiZaRd -- Fast compression method based on Lempel-Ziv 77
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/lizard.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>

struct lizard_buffer *
lizard_alloc(uns max_len)
{
  static byte *zero = "/dev/zero";
  int fd = open(zero, O_RDWR);
  if (fd < 0)
    die("open(%s): %m", zero);
  struct lizard_buffer *buf = xmalloc(sizeof(struct lizard_buffer));
  buf->len = ALIGN(max_len + PAGE_SIZE, PAGE_SIZE);
  buf->ptr = mmap(NULL, buf->len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (buf->ptr == MAP_FAILED)
    die("mmap(%s): %m", zero);
  return buf;
}

void
lizard_free(struct lizard_buffer *buf)
{
  munmap(buf->ptr, buf->len);
  xfree(buf);
}

static jmp_buf safe_decompress_jump;
static void
sigsegv_handler(int UNUSED whatsit)
{
  log(L_ERROR, "SIGSEGV caught in lizard_decompress()");
  longjmp(safe_decompress_jump, 1);
}

int
lizard_decompress_safe(byte *in, struct lizard_buffer *buf, uns expected_length)
  /* Decompresses into buf->ptr and returns the length of the uncompressed
   * file.  If an error has occured, -1 is returned and errno is set.  SIGSEGV
   * is caught in the case of buffer-overflow.  The function is not re-entrant
   * because of a static longjmp handler.  */
{
  volatile uns lock_offset = ALIGN(expected_length, PAGE_SIZE);
  if (lock_offset + PAGE_SIZE > buf->len)
  {
    errno = EFBIG;
    return -1;
  }
  mprotect(buf->ptr + lock_offset, PAGE_SIZE, PROT_NONE);
  volatile sighandler_t old_handler = signal(SIGSEGV, sigsegv_handler);
  int len, err;
  if (!setjmp(safe_decompress_jump))
  {
    len = lizard_decompress(in, buf->ptr);
    err = errno;
  }
  else
  {
    len = -1;
    err = EFAULT;
  }
  signal(SIGSEGV, old_handler);
  mprotect(buf->ptr + lock_offset, PAGE_SIZE, PROT_READ | PROT_WRITE);
  errno = err;
  return len;
}
