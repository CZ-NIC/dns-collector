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

#ifndef CONFIG_UCW_DEFAULT_CONFIG
#define CONFIG_UCW_DEFAULT_CONFIG NULL
#endif

#ifndef CONFIG_UCW_ENV_VAR_CONFIG
#define CONFIG_UCW_ENV_VAR_CONFIG NULL
#endif

struct cf_context *
cf_new_context(void)
{
  struct cf_context *cc = xmalloc_zero(sizeof(*cc));
  cc->need_journal = 1;
  clist_init(&cc->conf_entries);
  return cc;
}

void
cf_free_context(struct cf_context *cc)
{
  ASSERT(!cc->is_active);
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

struct cf_context *
cf_obtain_context(void)
{
  struct ucwlib_context *uc = ucwlib_thread_context();
  if (unlikely(!uc->cf_context))
    {
      struct cf_context *cc = cf_new_context();
      uc->cf_context = cc;
      cc->def_file = CONFIG_UCW_DEFAULT_CONFIG;
      cc->env_file = CONFIG_UCW_ENV_VAR_CONFIG;
      cc->is_active = 1;
    }
  return uc->cf_context;
}

void
cf_set_default_file(char *name)
{
  struct cf_context *cc = cf_obtain_context();
  cc->def_file = name;
}

void
cf_set_env_override(char *name)
{
  struct cf_context *cc = cf_obtain_context();
  cc->env_file = name;
}
