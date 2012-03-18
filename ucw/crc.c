/*
 *	CRC32 (Castagnoli 1993)
 *
 *	Based on Michael E. Kounavis and Frank L. Berry: A Systematic Approach
 *	to Building High Performance Software-based CRC Generators
 *	(Proceedings of the 10th IEEE Symposium on Computers and Communications 2005)
 *
 *	Includes code from http://sourceforge.net/projects/slicing-by-8/,
 *	which carried the following copyright notice:
 *
 *	Copyright (c) 2004-2006 Intel Corporation - All Rights Reserved
 *
 *	This software program is licensed subject to the BSD License,
 *	available at http://www.opensource.org/licenses/bsd-license.html
 *
 *	Adapted for LibUCW by Martin Mares <mj@ucw.cz> in 2012.
 */

#include <ucw/lib.h>
#include <ucw/crc.h>
#include <ucw/crc-tables.h>

static void crc32_update_by1(crc32_context *ctx, const byte *buf, uns len)
{
  u32 crc = ctx->state;
  while (len--)
    crc = crc_tableil8_o32[(crc ^ *buf++) & 0x000000FF] ^ (crc >> 8);
  ctx->state = crc;
}

static void crc32_update_by4(crc32_context *ctx, const byte *buf, uns len)
{
  uns init_bytes, words;
  u32 crc = ctx->state;
  u32 term1, term2, *buf32;

  // Align start address to a multiple of 4 bytes
  init_bytes = ((uintptr_t) buf) & 3;
  if (init_bytes)
    {
      init_bytes = 4 - init_bytes;
      len -= init_bytes;
      while (init_bytes--)
	crc = crc_tableil8_o32[(crc ^ *buf++) & 0x000000FF] ^ (crc >> 8);
    }

  // Process 4 bytes at a time
  words = len/4;
  len -= 4*words;
  buf32 = (u32 *) buf;
  while (words--)
    {
      crc ^= *buf32++;
      term1 = crc_tableil8_o56[crc & 0x000000FF] ^ crc_tableil8_o48[(crc >> 8) & 0x000000FF];
      term2 = crc >> 16;
      crc = term1 ^
	    crc_tableil8_o40[term2 & 0x000000FF] ^
	    crc_tableil8_o32[(term2 >> 8) & 0x000000FF];
    }

  // Process remaining up to 7 bytes
  buf = (byte *) buf32;
  while (len--)
    crc = crc_tableil8_o32[(crc ^ *buf++) & 0x000000FF] ^ (crc >> 8);

  ctx->state = crc;
}

static void crc32_update_by8(crc32_context *ctx, const byte *buf, uns len)
{
  uns init_bytes, quads;
  u32 crc = ctx->state;
  u32 term1, term2, *buf32;

  // Align start address to a multiple of 8 bytes
  init_bytes = ((uintptr_t) buf) & 7;
  if (init_bytes)
    {
      init_bytes = 8 - init_bytes;
      len -= init_bytes;
      while (init_bytes--)
	crc = crc_tableil8_o32[(crc ^ *buf++) & 0x000000FF] ^ (crc >> 8);
    }

  // Process 8 bytes at a time
  quads = len/8;
  len -= 8*quads;
  buf32 = (u32 *) buf;
  while (quads--)
    {
      crc ^= *buf32++;
      term1 = crc_tableil8_o88[crc & 0x000000FF] ^
	      crc_tableil8_o80[(crc >> 8) & 0x000000FF];
      term2 = crc >> 16;
      crc = term1 ^
	      crc_tableil8_o72[term2 & 0x000000FF] ^
	      crc_tableil8_o64[(term2 >> 8) & 0x000000FF];
      term1 = crc_tableil8_o56[*buf32 & 0x000000FF] ^
	      crc_tableil8_o48[(*buf32 >> 8) & 0x000000FF];

      term2 = *buf32 >> 16;
      crc =	crc ^
	      term1 ^
	      crc_tableil8_o40[term2  & 0x000000FF] ^
	      crc_tableil8_o32[(term2 >> 8) & 0x000000FF];
      buf32++;
    }

  // Process remaining up to 7 bytes
  buf = (byte *) buf32;
  while (len--)
    crc = crc_tableil8_o32[(crc ^ *buf++) & 0x000000FF] ^ (crc >> 8);

  ctx->state = crc;
}

void
crc32_init(crc32_context *ctx, uns crc_mode)
{
  ctx->state = 0xffffffff;
  switch (crc_mode)
    {
    case CRC_MODE_DEFAULT:
      ctx->update_func = crc32_update_by4;
      break;
    case CRC_MODE_SMALL:
      ctx->update_func = crc32_update_by1;
      break;
    case CRC_MODE_BIG:
      ctx->update_func = crc32_update_by8;
      break;
    default:
      ASSERT(0);
    }
}

u32
crc32_hash_buffer(const byte *buf, uns len)
{
  crc32_context ctx;
  crc32_init(&ctx, CRC_MODE_DEFAULT);
  crc32_update(&ctx, buf, len);
  return crc32_final(&ctx);
}

#ifdef TEST

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  if (argc != 5)
    die("Usage: crc-t <alg> <len> <block> <iters>");
  uns alg = atoi(argv[1]);
  uns len = atoi(argv[2]);
  uns block = atoi(argv[3]);
  uns iters = atoi(argv[4]);

  byte *buf = xmalloc(len);
  for (uns i=0; i<len; i++)
    buf[i] = i ^ (i >> 5) ^ (i >> 11);

  for (uns i=0; i<iters; i++)
    {
      crc32_context ctx;
      uns modes[] = { CRC_MODE_DEFAULT, CRC_MODE_SMALL, CRC_MODE_BIG };
      ASSERT(alg < ARRAY_SIZE(modes));
      crc32_init(&ctx, modes[alg]);
      for (uns p=0; p<len;)
	{
	  uns l = MIN(len-p, block);
	  crc32_update(&ctx, buf+p, l);
	  p += l;
	}
      uns crc = crc32_final(&ctx);
      if (!i)
	printf("%08x\n", crc);
    }

  return 0;
}

#endif
