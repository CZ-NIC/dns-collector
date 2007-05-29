/*
 *	UCW Library -- FastIO on files with run-time parametrization
 *
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/lfs.h"
#include "lib/fastbuf.h"

#include <fcntl.h>
#include <stdio.h>

struct fb_params fbpar_def = {
  .buffer_size = 65536,
}; 

struct cf_section fbpar_cf = {
# define F(x) PTR_TO(struct fb_params, x)
  CF_TYPE(struct fb_params),
  CF_ITEMS {
    // FIXME
    CF_LOOKUP("Type", (int *)F(type), ((byte *[]){"std", "direct", "mmap", NULL})),
    CF_UNS("BufSize", F(buffer_size)),
    CF_END
  }
# undef F
};

static struct cf_section fbpar_global_cf = {
  CF_ITEMS {
    CF_SECTION("Defaults", &fbpar_def, &fbpar_cf),
    CF_END
  }
};

static void CONSTRUCTOR
fbpar_global_init(void)
{
  cf_declare_section("FBParam", &fbpar_global_cf, 0);
}

static struct fastbuf *
bopen_fd_internal(int fd, struct fb_params *params, byte *name)
{
  struct fastbuf *fb;
  switch (params->type)
    {
      case FB_STD:
	return bfdopen_internal(fd, params->buffer_size, name);
      case FB_DIRECT:
	fb = fbdir_open_fd_internal(fd, params->asio, name);
	if (!fbdir_cheat && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_DIRECT) < 0)
          log(L_WARN, "Cannot set O_DIRECT on fd %d: %m", fd);
	return fb;
      case FB_MMAP:
	// FIXME
	ASSERT(0);
    }
  ASSERT(0);
}

static struct fastbuf *
bopen_file_internal(byte *name, int mode, struct fb_params *params, int try)
{
  if (params->type == FB_DIRECT && !fbdir_cheat)
    mode |= O_DIRECT;
  int fd = sh_open(name, mode, 0666);
  if (fd < 0)
    if (try)
      return NULL;
    else
      die("Unable to %s file %s: %m", (mode & O_CREAT) ? "create" : "open", name);
  struct fastbuf *fb = bopen_fd_internal(fd, params, name);
  ASSERT(fb);
  if (mode & O_APPEND)
    bseek(fb, 0, SEEK_END);
  return fb;
}

struct fastbuf *
bopen_file(byte *name, int mode, struct fb_params *params)
{
  return bopen_file_internal(name, mode, params ? : &fbpar_def, 0);
}

struct fastbuf *
bopen_file_try(byte *name, int mode, struct fb_params *params)
{
  return bopen_file_internal(name, mode, params ? : &fbpar_def, 1);
}

struct fastbuf *
bopen_fd(int fd, struct fb_params *params)
{
  byte x[32];
  sprintf(x, "fd%d", fd);
  return bopen_fd_internal(fd, params ? : &fbpar_def, x);
}

struct fastbuf *
bopen_tmp_file(struct fb_params *params)
{
  byte buf[TEMP_FILE_NAME_LEN];
  temp_file_name(buf);
  struct fastbuf *fb = bopen_file_internal(buf, O_RDWR | O_CREAT | O_TRUNC, params, 0);
  bconfig(fb, BCONFIG_IS_TEMP_FILE, 1);
  return fb;
}
