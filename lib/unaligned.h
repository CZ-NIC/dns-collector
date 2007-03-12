/*
 *	UCW Library -- Fast Access to Unaligned Data
 *
 *	(c) 1997--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_UNALIGNED_H
#define _UCW_UNALIGNED_H

/* Big endian format */

#if defined(CPU_ALLOW_UNALIGNED) && defined(CPU_BIG_ENDIAN)
static inline uns get_u16_be(byte *p) { return *(u16 *)p; }
static inline u32 get_u32_be(byte *p) { return *(u32 *)p; }
static inline u64 get_u64_be(byte *p) { return *(u64 *)p; }
static inline void put_u16_be(byte *p, uns x) { *(u16 *)p = x; }
static inline void put_u32_be(byte *p, u32 x) { *(u32 *)p = x; }
static inline void put_u64_be(byte *p, u64 x) { *(u64 *)p = x; }
#else
static inline uns get_u16_be(byte *p)
{
  return (p[0] << 8) | p[1];
}
static inline u32 get_u32_be(byte *p)
{
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
static inline u64 get_u64_be(byte *p)
{
  return ((u64) get_u32_be(p) << 32) | get_u32_be(p+4);
}
static inline void put_u16_be(byte *p, uns x)
{
  p[0] = x >> 8;
  p[1] = x;
}
static inline void put_u32_be(byte *p, u32 x)
{
  p[0] = x >> 24;
  p[1] = x >> 16;
  p[2] = x >> 8;
  p[3] = x;
}
static inline void put_u64_be(byte *p, u64 x)
{
  put_u32_be(p, x >> 32);
  put_u32_be(p+4, x);
}
#endif

/* Little-endian format */

#if defined(CPU_ALLOW_UNALIGNED) && !defined(CPU_BIG_ENDIAN)
static inline uns get_u16_le(byte *p) { return *(u16 *)p; }
static inline u32 get_u32_le(byte *p) { return *(u32 *)p; }
static inline u64 get_u64_le(byte *p) { return *(u64 *)p; }
static inline void put_u16_le(byte *p, uns x) { *(u16 *)p = x; }
static inline void put_u32_le(byte *p, u32 x) { *(u32 *)p = x; }
static inline void put_u64_le(byte *p, u64 x) { *(u64 *)p = x; }
#else
static inline uns get_u16_le(byte *p)
{
  return p[0] | (p[1] << 8);
}
static inline u32 get_u32_le(byte *p)
{
  return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}
static inline u64 get_u64_le(byte *p)
{
  return get_u32_le(p) | ((u64) get_u32_le(p+4) << 32);
}
static inline void put_u16_le(byte *p, uns x)
{
  p[0] = x;
  p[1] = x >> 8;
}
static inline void put_u32_le(byte *p, u32 x)
{
  p[0] = x;
  p[1] = x >> 8;
  p[2] = x >> 16;
  p[3] = x >> 24;
}
static inline void put_u64_le(byte *p, u64 x)
{
  put_u32_le(p, x);
  put_u32_le(p+4, x >> 32);
}
#endif

static inline u64 get_u40_be(byte *p)
{
  return ((u64)p[0] << 32) | get_u32_be(p+1);
}

static inline void put_u40_be(byte *p, u64 x)
{
  p[0] = x >> 32;
  put_u32_be(p+1, x);
}

static inline u64 get_u40_le(byte *p)
{
  return get_u32_le(p) | ((u64) p[4] << 32);
}

static inline void put_u40_le(byte *p, u64 x)
{
  put_u32_le(p, x);
  p[4] = x >> 32;
}

/* The native format */

#ifdef CPU_BIG_ENDIAN

static inline uns get_u16(byte *p) { return get_u16_be(p); }
static inline u32 get_u32(byte *p) { return get_u32_be(p); }
static inline u64 get_u64(byte *p) { return get_u64_be(p); }
static inline u64 get_u40(byte *p) { return get_u40_be(p); }
static inline void put_u16(byte *p, uns x) { return put_u16_be(p, x); }
static inline void put_u32(byte *p, u32 x) { return put_u32_be(p, x); }
static inline void put_u64(byte *p, u64 x) { return put_u64_be(p, x); }
static inline void put_u40(byte *p, u64 x) { return put_u40_be(p, x); }

#else

static inline uns get_u16(byte *p) { return get_u16_le(p); }
static inline u32 get_u32(byte *p) { return get_u32_le(p); }
static inline u64 get_u64(byte *p) { return get_u64_le(p); }
static inline u64 get_u40(byte *p) { return get_u40_le(p); }
static inline void put_u16(byte *p, uns x) { return put_u16_le(p, x); }
static inline void put_u32(byte *p, u32 x) { return put_u32_le(p, x); }
static inline void put_u64(byte *p, u64 x) { return put_u64_le(p, x); }
static inline void put_u40(byte *p, u64 x) { return put_u40_le(p, x); }

#endif

/* Just for completeness */

static inline uns get_u8(byte *p) { return *p; }
static inline void put_u8(byte *p, uns x) { *p = x; }

/* Backward compatibility macros */

#define GET_U8(p) get_u8((byte*)(p))
#define GET_U16(p) get_u16((byte*)(p))
#define GET_U32(p) get_u32((byte*)(p))
#define GET_U64(p) get_u64((byte*)(p))
#define GET_U40(p) get_u40((byte*)(p))

#define PUT_U8(p,x) put_u8((byte*)(p),x);
#define PUT_U16(p,x) put_u16((byte*)(p),x)
#define PUT_U32(p,x) put_u32((byte*)p,x)
#define PUT_U64(p,x) put_u64((byte*)p,x)
#define PUT_U40(p,x) put_u40((byte*)p,x)

#endif
