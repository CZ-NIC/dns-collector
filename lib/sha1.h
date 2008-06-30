/*
 *	SHA-1 Hash Function (FIPS 180-1, RFC 3174)
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	Based on the code from libgcrypt-1.2.3, which was:
 *
 *	Copyright (C) 1998, 2001, 2002, 2003 Free Software Foundation, Inc.
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_SHA1_H
#define _UCW_SHA1_H

typedef struct {
  u32 h0,h1,h2,h3,h4;
  u32 nblocks;
  byte buf[64];
  int count;
} sha1_context;

void sha1_init(sha1_context *hd);
void sha1_update(sha1_context *hd, const byte *inbuf, uns inlen);
byte *sha1_final(sha1_context *hd);

/* One-shot interface */
void sha1_hash_buffer(byte *outbuf, const byte *buffer, uns length);

#define SHA1_SIZE 20
#define SHA1_HEX_SIZE 41

#endif
