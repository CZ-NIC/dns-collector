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
#include "lib/fastbuf.h"

struct fb_params fbpar_defaults = {
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
    CF_SECTION("Defaults", &fbpar_defaults, &fbpar_cf),
    CF_END
  }
};

static void CONSTRUCTOR
fbpar_global_init(void)
{
  cf_declare_section("FBParam", &fbpar_global_cf, 0);
}

struct fastbuf *
bopen_file(byte *name, int mode, struct fb_params *params)
{
  params = params ? : &fbpar_defaults;
  switch (params->type)
    {
      case FB_STD:
        return bopen(name, mode, params->buffer_size);
      case FB_DIRECT:
        return fbdir_open(name, mode, NULL);
      default:
	ASSERT(0);
    }
}

struct fastbuf *
bopen_file_try(byte *name, int mode, struct fb_params *params)
{
  params = params ? : &fbpar_defaults;
  switch (params->type)
    {
      case FB_STD:
        return bopen_try(name, mode, params->buffer_size);
      case FB_DIRECT:
        return fbdir_open_try(name, mode, NULL);
      default:
	ASSERT(0);
    }
}

struct fastbuf *
bopen_fd(int fd, struct fb_params *params)
{
  params = params ? : &fbpar_defaults;
  switch (params->type)
    {
      case FB_STD:
        return bfdopen(fd, params->buffer_size);
      case FB_DIRECT:
        return fbdir_open_fd(fd, NULL);
      default:
	ASSERT(0);
    }
}

struct fastbuf *
bopen_tmp_file(struct fb_params *params)
{
  params = params ? : &fbpar_defaults;
  switch (params->type)
    {
      case FB_STD:
        return bopen_tmp(params->buffer_size);
      case FB_DIRECT:
        return fbdir_open_tmp(NULL);
      default:
	ASSERT(0);
    }
}
