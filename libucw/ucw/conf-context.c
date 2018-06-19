/*
 *	UCW Library -- Configuration files: Contexts
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/conf-internal.h>
#include <ucw/threads.h>

static struct cf_context cf_default_context;

static void
cf_init_context(struct cf_context *cc)
{
  cc->enable_journal = 1;
  clist_init(&cc->conf_entries);
}

struct cf_context *
cf_new_context(void)
{
  struct cf_context *cc = xmalloc_zero(sizeof(*cc));
  cf_init_context(cc);
  return cc;
}

void
cf_delete_context(struct cf_context *cc)
{
  ASSERT(!cc->is_active);
  ASSERT(cc != &cf_default_context);
  struct cf_context *prev = cf_switch_context(cc);
  cf_revert();
  cf_switch_context(prev);
  xfree(cc->parser);
  xfree(cc);
}

struct cf_context *
cf_switch_context(struct cf_context *cc)
{
  struct ucwlib_context *uc = ucwlib_thread_context();
  struct cf_context *prev = uc->cf_context;
  if (prev)
    prev->is_active = 0;
  if (cc)
    {
      ASSERT(!cc->is_active);
      cc->is_active = 1;
    }
  uc->cf_context = cc;
  return prev;
}

static void CONSTRUCTOR_WITH_PRIORITY(10100)
cf_init_default_context(void)
{
  cf_init_context(&cf_default_context);
  ucwlib_thread_context()->cf_context = &cf_default_context;
  cf_default_context.is_active = 1;
}

struct cf_context *
cf_obtain_context(void)
{
  return cf_get_context();
}
