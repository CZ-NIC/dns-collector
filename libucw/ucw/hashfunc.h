/*
 *	UCW Library -- Hyper-super-meta-alt-control-shift extra fast
 *	str_len() and hash_*() routines
 *
 *	(c) 2002, Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_HASHFUNC_H
#define _UCW_HASHFUNC_H

#include <ucw/lib.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define hash_block ucw_hash_block
#define hash_block_aligned ucw_hash_block_aligned
#define hash_string ucw_hash_string
#define hash_string_aligned ucw_hash_string_aligned
#define hash_string_nocase ucw_hash_string_nocase
#define str_len ucw_str_len
#define str_len_aligned ucw_str_len_aligned
#endif

/*** === String hashes [[strhash]] ***/

/* The following functions need str to be aligned to sizeof(uint).  */
uint str_len_aligned(const char *str) PURE; /** Get the string length (not a really useful hash function, but there is no better place for it). The string must be aligned to sizeof(uint). For unaligned see @str_len(). **/
uint hash_string_aligned(const char *str) PURE; /** Hash the string. The string must be aligned to sizeof(uint). For unaligned see @hash_string(). **/
uint hash_block_aligned(const byte *buf, uint len) PURE; /** Hash arbitrary data. They must be aligned to sizeof(uint). For unaligned see @hash_block(). **/

#ifdef CPU_ALLOW_UNALIGNED
#undef str_len
#undef hash_string
#undef hash_block
#define	str_len(str)		str_len_aligned(str)
#define	hash_string(str)	hash_string_aligned(str)
#define	hash_block(str, len)	hash_block_aligned(str, len)
#else
uint str_len(const char *str) PURE; /** Get the string length. If you know it is aligned to sizeof(uint), you can use faster @str_len_aligned(). **/
uint hash_string(const char *str) PURE; /** Hash the string. If it is aligned to sizeof(uint), you can use faster @hash_string_aligned(). **/
uint hash_block(const byte *buf, uint len) PURE; /** Hash arbitrary data. If they are aligned to sizeof(uint), use faster @hash_block_aligned(). **/
#endif

uint hash_string_nocase(const char *str) PURE; /** Hash the string in a case insensitive way. Works only with ASCII characters. **/

/*** === Integer hashes [[inthash]] ***/

/***
 * We hash integers by multiplying by a reasonably large prime with
 * few ones in its binary form (to give the compiler the possibility
 * of using shifts and adds on architectures where multiplication
 * instructions are slow).
 */
static inline uint CONST hash_u32(uint x) { return 0x01008041*x; } /** Hash a 32 bit unsigned integer. **/
static inline uint CONST hash_u64(u64 x) { return hash_u32((uint)x ^ (uint)(x >> 32)); } /** Hash a 64 bit unsigned integer. **/
static inline uint CONST hash_pointer(void *x) { return ((sizeof(x) <= 4) ? hash_u32((uint)(uintptr_t)x) : hash_u64((u64)(uintptr_t)x)); } /** Hash a pointer. **/

#endif
