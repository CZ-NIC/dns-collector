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

/*
 * Internal CRC calculator context.
 * You should use it just as an opaque handle only.
 */
typedef struct crc32_context {
  u32 state;
  void (*update_func)(struct crc32_context *ctx, const byte *buf, uns len);
} crc32_context;

void crc32_init(crc32_context *ctx, uns crc_mode);

enum crc_mode {
  CRC_MODE_DEFAULT,
  CRC_MODE_SMALL,
  CRC_MODE_BIG,
  CRC_MODE_MAX,
};

static inline void
crc32_update(crc32_context *ctx, const byte *buf, uns len)
{
  ctx->update_func(ctx, buf, len);
}

static inline u32
crc32_final(crc32_context *ctx)
{
  return ctx->state ^ 0xffffffff;
}
