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

struct lizard_buffer {
  uns len;
  void *ptr;
  struct sigaction *old_sigsegv_handler;
};

struct lizard_buffer *
lizard_alloc(void)
{
  struct lizard_buffer *buf = xmalloc(sizeof(struct lizard_buffer));
  buf->len = 0;
  buf->ptr = NULL;
  buf->old_sigsegv_handler = xmalloc(sizeof(struct sigaction));
  handle_signal(SIGSEGV, buf->old_sigsegv_handler);
  return buf;
}

void
lizard_free(struct lizard_buffer *buf)
{
  if (buf->ptr)
    munmap(buf->ptr, buf->len + PAGE_SIZE);
  unhandle_signal(SIGSEGV, buf->old_sigsegv_handler);
  xfree(buf->old_sigsegv_handler);
  xfree(buf);
}

static void
lizard_realloc(struct lizard_buffer *buf, uns max_len)
  /* max_len needs to be aligned to PAGE_SIZE */
{
  if (max_len <= buf->len)
    return;
  if (max_len < 2*buf->len)				// to ensure logarithmic cost
    max_len = 2*buf->len;

  if (buf->ptr)
    munmap(buf->ptr, buf->len + PAGE_SIZE);
  buf->len = max_len;
  buf->ptr = mmap(NULL, buf->len + PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (buf->ptr == MAP_FAILED)
    die("mmap(anonymous): %m");
  if (mprotect(buf->ptr + buf->len, PAGE_SIZE, PROT_NONE) < 0)
    die("mprotect: %m");
}

static jmp_buf safe_decompress_jump;
static int
sigsegv_handler(int signal UNUSED)
{
  log(L_ERROR, "SIGSEGV caught in lizard_decompress()");
  longjmp(safe_decompress_jump, 1);
  return 1;
}

int
lizard_decompress_safe(byte *in, struct lizard_buffer *buf, uns expected_length, byte **ptr)
  /* Decompresses in into buf, sets *ptr to the data, and returns the
   * uncompressed length.  If an error has occured, -1 is returned and errno is
   * set.  The buffer buf is automatically reallocated.  SIGSEGV is caught in
   * case of buffer-overflow.  The function is not re-entrant because of a
   * static longjmp handler.  */
{
  uns lock_offset = ALIGN(expected_length + 3, PAGE_SIZE);	// +3 due to the unaligned access
  if (lock_offset > buf->len)
    lizard_realloc(buf, lock_offset);
  volatile sh_sighandler_t old_handler = signal_handler[SIGSEGV];
  signal_handler[SIGSEGV] = sigsegv_handler;
  int len;
  if (!setjmp(safe_decompress_jump))
  {
    *ptr = buf->ptr + buf->len - lock_offset;
    len = lizard_decompress(in, *ptr);
  }
  else
  {
    *ptr = NULL;
    len = -1;
    errno = EFAULT;
  }
  signal_handler[SIGSEGV] = old_handler;
  return len;
}
