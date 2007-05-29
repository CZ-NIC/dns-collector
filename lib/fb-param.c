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
  .read_ahead = 1,
  .write_back = 1,
}; 

struct cf_section fbpar_cf = {
# define F(x) PTR_TO(struct fb_params, x)
  CF_TYPE(struct fb_params),
  CF_ITEMS {
    CF_LOOKUP("Type", (int *)F(type), ((byte *[]){"std", "direct", "mmap", NULL})),
    CF_UNS("BufSize", F(buffer_size)),
    CF_UNS("ReadAhead", F(read_ahead)),
    CF_UNS("WriteBack", F(write_back)),
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
bopen_fd_internal(int fd, struct fb_params *params, uns mode, byte *name)
{
  byte buf[32];
  if (!name)
    sprintf(name = buf, "fd%d", fd);
  struct fastbuf *fb;
  switch (params->type)
    {
      case FB_STD:
	return bfdopen_internal(fd, name,
	    params->buffer_size ? : fbpar_def.buffer_size);
      case FB_DIRECT:
	fb = fbdir_open_fd_internal(fd, name, params->asio,
	    params->buffer_size ? : fbpar_def.buffer_size,
	    params->read_ahead ? : fbpar_def.read_ahead,
	    params->write_back ? : fbpar_def.write_back);
	if (!~mode && !fbdir_cheat && ((int)(mode = fcntl(fd, F_GETFL)) < 0 || fcntl(fd, F_SETFL, mode | O_DIRECT)) < 0)
          log(L_WARN, "Cannot set O_DIRECT on fd %d: %m", fd);
	return fb;
      case FB_MMAP:
	if (!~mode && (int)(mode = fcntl(fd, F_GETFL)) < 0)
          die("Cannot get flags of fd %d: %m", fd);
	return bfmmopen_internal(fd, name, mode);
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
  struct fastbuf *fb = bopen_fd_internal(fd, params, mode, name);
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
  return bopen_fd_internal(fd, params ? : &fbpar_def, ~0U, NULL);
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
