/*
 *	UCW Library -- Fast Access to Unaligned Data
 *
 *	(c) 1997--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_UNALIGNED_H
#define _UCW_UNALIGNED_H

#ifdef CPU_ALLOW_UNALIGNED
#define GET_U16(p) (*((u16 *)(p)))
#define GET_U32(p) (*((u32 *)(p)))
#define GET_U32_16(p) (*((u32 *)(p)))
#define GET_U64(p) (*((u64 *)(p)))
#define PUT_U16(p,x) *((u16 *)(p)) = (x)
#define PUT_U32(p,x) *((u32 *)(p)) = (x)
#define PUT_U64(p,x) *((u64 *)(p)) = (x)
#else
#define GET_U64(p) ({ u64 _x; memcpy(&_x, p, 8); _x; })
#define PUT_U64(p,x) do { u64 _x=x; memcpy(p, &_x, 8); } while(0)
#ifdef CPU_BIG_ENDIAN
#define GET_U16(p) (((p)[0] << 8) | (p)[1])
#define GET_U32(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])
#define PUT_U16(p,x) (void)(((p)[0] = ((x) >> 8)), (((p)[1]) = (x)))
#define PUT_U32(p,x) (void)(((p)[0] = ((x) >> 24)), (((p)[1]) = ((x) >> 16)), (((p)[2]) = ((x) >> 8)), (((p)[3]) = (x)))
#else
#define GET_U16(p) (((p)[1] << 8) | (p)[0])
#define GET_U32(p) (((p)[3] << 24) | ((p)[2] << 16) | ((p)[1] << 8) | (p)[0])
#define PUT_U16(p,x) (void)(((p)[1] = ((x) >> 8)), (((p)[0]) = (x)))
#define PUT_U32(p,x) (void)(((p)[3] = ((x) >> 24)), (((p)[2]) = ((x) >> 16)), (((p)[1]) = ((x) >> 8)), (((p)[0]) = (x)))
#endif
#endif

#define GET_U32_BE16(p) (((p)[0] << 16) | (p)[1])

#ifdef CPU_BIG_ENDIAN
#define GET_U40(p) (((u64) (p)[0] << 32) | GET_U32((p)+1))
#define PUT_U40(p,x) do { (p)[0] = ((x) >> 32); PUT_U32(((p)+1), x); } while(0)
#else
#define GET_U40(p) (((u64) (p)[4] << 32) | GET_U32(p))
#define PUT_U40(p,x) do { (p)[4] = ((x) >> 32); PUT_U32(p, x); } while(0)
#endif

/* Just for completeness */

#define GET_U8(p) (*(byte *)(p))
#define PUT_U8(p,x) *((byte *)(p)) = (x)

#endif
