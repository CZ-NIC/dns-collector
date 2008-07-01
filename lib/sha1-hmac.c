/*
 *	HMAC-SHA1 Message Authentication Code (RFC 2202)
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/sha1.h"

#include <string.h>

void
sha1_hmac(byte *outbuf, const byte *key, uns keylen, const byte *data, uns datalen)
{
  sha1_context ictx, octx;
  byte keybuf[SHA1_BLOCK_SIZE], buf[SHA1_BLOCK_SIZE];

  // Hash the key if necessary
  if (keylen <= SHA1_BLOCK_SIZE)
    {
      memcpy(keybuf, key, keylen);
      bzero(keybuf + keylen, SHA1_BLOCK_SIZE - keylen);
    }
  else
    {
      sha1_hash_buffer(keybuf, key, keylen);
      bzero(keybuf + SHA1_SIZE, SHA1_BLOCK_SIZE - SHA1_SIZE);
    }

  // The inner digest
  sha1_init(&ictx);
  for (int i=0; i < SHA1_BLOCK_SIZE; i++)
    buf[i] = keybuf[i] ^ 0x36;
  sha1_update(&ictx, buf, SHA1_BLOCK_SIZE);
  sha1_update(&ictx, data, datalen);
  byte *isha = sha1_final(&ictx);

  // The outer digest
  sha1_init(&octx);
  for (int i=0; i < SHA1_BLOCK_SIZE; i++)
    buf[i] = keybuf[i] ^ 0x5c;
  sha1_update(&octx, buf, SHA1_BLOCK_SIZE);
  sha1_update(&octx, isha, SHA1_SIZE);
  byte *osha = sha1_final(&octx);

  // Copy the result
  memcpy(outbuf, osha, SHA1_SIZE);
}

#ifdef TEST

#include <stdio.h>
#include "lib/string.h"

static uns rd(char *dest)
{
  char buf[1024];
  fgets(buf, sizeof(buf), stdin);
  *strchr(buf, '\n') = 0;
  if (buf[0] == '0' && buf[1] == 'x')
    {
      const char *e = hex_to_mem(dest, buf+2, 1024, 0);
      ASSERT(!*e);
      return (e-buf-2)/2;
    }
  else
    {
      strcpy(dest, buf);
      return strlen(dest);
    }
}

int main(void)
{
  char key[1024], data[1024];
  byte hmac[SHA1_SIZE];
  uns kl = rd(key);
  uns dl = rd(data);
  sha1_hmac(hmac, key, kl, data, dl);
  mem_to_hex(data, hmac, SHA1_SIZE, 0);
  puts(data);
  return 0;
}

#endif
