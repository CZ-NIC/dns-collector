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

#ifndef _UCW_CRC_H
#define _UCW_CRC_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define crc32_hash_buffer ucw_crc32_hash_buffer
#define crc32_init ucw_crc32_init
#endif

/**
 * Internal CRC calculator context.
 * You should use it just as an opaque handle only.
 */
typedef struct crc32_context {
  u32 state;
  void (*update_func)(struct crc32_context *ctx, const byte *buf, uns len);
} crc32_context;

/**
 * Initialize new calculation of CRC in a given context.
 * @crc_mode selects which algorithm should be used.
 **/
void crc32_init(crc32_context *ctx, uns crc_mode);

/**
 * Algorithm used for CRC calculation. The algorithms differ by the amount
 * of precomputed tables they use. Bigger tables imply faster calculation
 * at the cost of an increased cache footprint.
 **/
enum crc_mode {
  CRC_MODE_DEFAULT,		/* Default algorithm (4K table) */
  CRC_MODE_SMALL,		/* Optimize for small data (1K table) */
  CRC_MODE_BIG,			/* Optimize for large data (8K table) */
  CRC_MODE_MAX,
};

/** Feed @len bytes starting at @buf to the CRC calculator. **/
static inline void crc32_update(crc32_context *ctx, const byte *buf, uns len)
{
  ctx->update_func(ctx, buf, len);
}

/** Finish calculation and return the CRC value. **/
static inline u32 crc32_final(crc32_context *ctx)
{
  return ctx->state ^ 0xffffffff;
}

/**
 * A convenience one-shot function for CRC.
 * It is equivalent to this snippet of code:
 *
 *  crc32_context ctx;
 *  crc32_init(&ctx, CRC_MODE_DEFAULT);
 *  crc32_update(&ctx, buf, len);
 *  return crc32_final(&ctx);
 */
u32 crc32_hash_buffer(const byte *buf, uns len);

#endif
