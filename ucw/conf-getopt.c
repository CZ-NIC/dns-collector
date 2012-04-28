/*
 *	UCW Library -- Configuration files: getopt wrapper
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/conf-internal.h>
#include <ucw/getopt.h>
#include <ucw/fastbuf.h>

#include <stdlib.h>

#ifndef CONFIG_UCW_DEFAULT_CONFIG
#define CONFIG_UCW_DEFAULT_CONFIG NULL
#endif
char *cf_def_file = CONFIG_UCW_DEFAULT_CONFIG;

#ifndef CONFIG_UCW_ENV_VAR_CONFIG
#define CONFIG_UCW_ENV_VAR_CONFIG NULL
#endif
char *cf_env_file = CONFIG_UCW_ENV_VAR_CONFIG;

static void
load_default(struct cf_context *cc)
{
  if (cc->config_loaded++)
    return;
  if (cf_def_file)
    {
      char *env;
      if (cf_env_file && (env = getenv(cf_env_file)))
        {
	  if (cf_load(env))
	    die("Cannot load config file %s", env);
	}
      else if (cf_load(cf_def_file))
        die("Cannot load default config %s", cf_def_file);
    }
  else
    {
      // We need to create an empty pool and initialize all configuration items
      struct cf_journal_item *oldj = cf_journal_new_transaction(1);
      cf_init_stack(cc);
      cf_done_stack(cc);
      cf_journal_commit_transaction(1, oldj);
    }
}

static void
end_of_options(struct cf_context *cc)
{
  load_default(cc);
  if (cc->postpone_commit && cf_close_group())
    die("Cannot commit after the initialization");
}

int
cf_getopt(int argc, char *const argv[], const char *short_opts, const struct option *long_opts, int *long_index)
{
  struct cf_context *cc = cf_get_context();
  if (!cc->postpone_commit)
    cf_open_group();

  while (1)
    {
      int res = getopt_long(argc, argv, short_opts, long_opts, long_index);
      if (res == 'S' || res == 'C' || res == 0x64436667)
	{
	  if (cc->other_options)
	    die("The -S and -C options must precede all other arguments");
	  if (res == 'S')
	    {
	      load_default(cc);
	      if (cf_set(optarg))
		die("Cannot set %s", optarg);
	    }
	  else if (res == 'C')
	    {
	      if (cf_load(optarg))
		die("Cannot load config file %s", optarg);
	    }
#ifdef CONFIG_UCW_DEBUG
	  else
	    {			/* --dumpconfig */
	      end_of_options(cc);
	      struct fastbuf *b = bfdopen(1, 4096);
	      cf_dump_sections(b);
	      bclose(b);
	      exit(0);
	    }
#endif
	}
      else
	{
	  /* unhandled option or end of options */
	  if (res != ':' && res != '?')
	    end_of_options(cc);
	  cc->other_options++;
	  return res;
	}
    }
}
