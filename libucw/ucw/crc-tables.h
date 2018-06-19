/*
 *	CRC32 (Castagnoli 1993) -- Tables
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

#ifdef CONFIG_UCW_CLEAN_ABI
#define crc_tableil8_o32 ucw_crc_tableil8_o32
#define crc_tableil8_o40 ucw_crc_tableil8_o40
#define crc_tableil8_o48 ucw_crc_tableil8_o48
#define crc_tableil8_o56 ucw_crc_tableil8_o56
#define crc_tableil8_o64 ucw_crc_tableil8_o64
#define crc_tableil8_o72 ucw_crc_tableil8_o72
#define crc_tableil8_o80 ucw_crc_tableil8_o80
#define crc_tableil8_o88 ucw_crc_tableil8_o88
#endif

extern const u32 crc_tableil8_o32[256];
extern const u32 crc_tableil8_o40[256];
extern const u32 crc_tableil8_o48[256];
extern const u32 crc_tableil8_o56[256];
extern const u32 crc_tableil8_o64[256];
extern const u32 crc_tableil8_o72[256];
extern const u32 crc_tableil8_o80[256];
extern const u32 crc_tableil8_o88[256];
