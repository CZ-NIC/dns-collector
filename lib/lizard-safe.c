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
  struct lizard_buffer *buf = xmalloc(sizeof(struct lizard_buffer));
  buf->len = ALIGN(max_len + 3, PAGE_SIZE);		// +3 due to the unaligned access
  buf->start = mmap(NULL, buf->len + PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (buf->start == MAP_FAILED)
    die("mmap(anonymous): %m");
  if (mprotect(buf->start + buf->len, PAGE_SIZE, PROT_NONE) < 0)
    die("mprotect: %m");
  buf->old_sigsegv_handler = handle_signal(SIGSEGV);
  return buf;
}

void
lizard_free(struct lizard_buffer *buf)
{
  munmap(buf->start, buf->len + PAGE_SIZE);
  signal(SIGSEGV, buf->old_sigsegv_handler);
  xfree(buf);
}

static jmp_buf safe_decompress_jump;
static void
sigsegv_handler(void)
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
  uns lock_offset = ALIGN(expected_length + 3, PAGE_SIZE);	// +3 due to the unaligned access
  if (lock_offset > buf->len)
  {
    errno = EFBIG;
    return -1;
  }
  volatile my_sighandler_t old_handler = signal_handler[SIGSEGV];
  signal_handler[SIGSEGV] = sigsegv_handler;
  int len, err;
  if (!setjmp(safe_decompress_jump))
  {
    buf->ptr = buf->start + buf->len - lock_offset;
    len = lizard_decompress(in, buf->ptr);
    err = errno;
  }
  else
  {
    buf->ptr = NULL;
    len = -1;
    err = EFAULT;
  }
  signal_handler[SIGSEGV] = old_handler;
  errno = err;
  return len;
}
