/*
 *	Sherlock Library -- URL Keys & URL Fingerprints
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/index.h"
#include "lib/url.h"

#include <string.h>

static uns urlkey_www_hack;

static struct cfitem urlkey_config[] = {
  { "URLKey",		CT_SECTION,	NULL },
  { "WWWHack",		CT_INT,		&urlkey_www_hack },
  { NULL,		CT_STOP,	NULL }
};

static void CONSTRUCTOR urlkey_conf_init(void)
{
  cf_register(urlkey_config);
}

byte *
url_key(byte *url, byte *buf)
{
  if (urlkey_www_hack && !strncmp(url, "http://www.", 11))
    {
      strcpy(buf, "http://");
      strcpy(buf+7, url+11);
      return buf;
    }
  else
    return url;
}

void
url_fingerprint(byte *url, struct fingerprint *fp)
{
  byte buf[MAX_URL_SIZE];
  return fingerprint(url_key(url, buf), fp);
}
