/*
 *	LiZzaRd -- Fast compression method based on Lempel-Ziv 77
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	The file format is based on LZO1X and 
 *	the compression method is based on zlib.
 */

#include "lib/lib.h"
#include "lib/lizzard.h"

#include <string.h>

typedef u16 hash_ptr_t;
struct hash_record {
  byte *string;				// in original text
  hash_ptr_t next4, next3;		// 0=end
};

#define	HASH_SIZE	((1<<14) + 27)	// prime, size of hash-table
#define	HASH_RECORDS	((1<<15) - 1)	// maximum number of records in hash-table
#define	CHAIN_MAX_TESTS	16		// crop longer collision chains
#define	CHAIN_GOOD_MATCH	16	// we already have a good match => end

static inline uns
hashf(byte *string)
{
    uns hash;
#ifdef	CPU_ALLOW_UNALIGNED
    hash = * (uns *) string;
#elif	CPU_LITTLE_ENDIAN
    hash = string[0] | (string[1]<<8) | (string[2]<<16) | (string[3]<<24);
#else
    hash = string[3] | (string[2]<<8) | (string[1]<<16) | (string[0]<<24);
#endif
    return hash;
}
#ifdef	CPU_LITTLE_ENDIAN
#define	MASK_4TH_CHAR	(~(0xff << 24))
#else
#define	MASK_4TH_CHAR	(~0xff)
#endif

static inline uns
find_match(uns list_id, hash_ptr_t *hash_tab, struct hash_record *hash_rec, uns hash, byte *string, byte *string_end, byte **best_ptr)
  /* We heavily rely on gcc optimisations (e.g. list_id is a constant hence
   * some branches of the code will be pruned).
   *
   * hash_tab[hash] points to the 1st record of the link-list of strings with the
   * same hash.  The records are statically stored in circular array hash_rec
   * (with 1st entry unused), and the pointers are just 16-bit indices.  In one
   * array, we store 2 independent link-lists: with 3 and 4 hashed characters.
   *
   * Updates are performed more often than searches, so they are lazy: we only
   * add new records in front of link-lists and never delete them.  By reusing
   * an already allocated space in the circular array we necessarily modify
   * records that are already pointed at in the middle of some link-list.  To
   * prevent confusion, this function checks 2 invariants: the strings in a
   * collision chain are ordered, and the hash of the 1st record in the chain
   * must match.  */
{
  if (list_id == 3)				/* Find the 1st record in the collision chain.  */
    hash = hash & MASK_4TH_CHAR;
  hash %= HASH_SIZE;
  if (!hash_tab[hash])
    return 0;
  struct hash_record *record = hash_rec + hash_tab[hash];
  uns hash2 = hashf(record->string);
  if (list_id == 3)
    hash2 = hash2 & MASK_4TH_CHAR;
  hash2 %= HASH_SIZE;
  if (hash2 != hash)				/* Verify the hash-value.  */
  {
    hash_tab[hash] = 0;				/* prune */
    return 0;
  }

  uns count = CHAIN_MAX_TESTS;
  uns best_len = 0;
  byte *last_string = string_end;
  hash_ptr_t *last_ptr = NULL;
  while (count-- > 0)
  {
    byte *cmp = record->string;
    if (cmp >= last_string)			/* collision chain is ordered by age */
    {
      *last_ptr = 0;				/* prune */
      break;
    }
    if (cmp[0] == string[0] && cmp[2] == string[2])
    {
      if (cmp[3] == string[3])
      {
	/* implies cmp[1] == string[1], becase we hash into 14+eps bits */
	cmp += 4;
	if (*cmp++ == string[4] && *cmp++ == string[5]
	    && *cmp++ == string[6] && *cmp++ == string[7])
	{
	  byte *str = string + 8;
	  while (str <= string_end && *cmp++ == *string++);
	}
      }
      else if (list_id == 3)
	/* hashing 3 characters implies cmp[1] == string[1]
	   hashing 4 characters implies cmp[1] != string[1] */
	cmp += 4;
      else
	goto next_collision;
      uns len = cmp - record->string - 1;	/* cmp points 2 characters after the last match */
      if (len > best_len)
      {
	best_len = len;
	*best_ptr = record->string;
	if (best_len >= CHAIN_GOOD_MATCH)	/* optimisation */
	  break;
      }
    }
next_collision:
    if (list_id == 3)
    {
      if (!record->next3)
	break;
      last_ptr = &record->next3;
      record = hash_rec + record->next3;
    }
    else
    {
      if (!record->next4)
	break;
      last_ptr = &record->next4;
      record = hash_rec + record->next4;
    }
  }
  return best_len;
}

static inline uns
hash_string(byte *string, uns hash, hash_ptr_t *tab3, hash_ptr_t *tab4, struct hash_record *hash_rec, uns head)
{
  uns h3 = (hash & MASK_4TH_CHAR) % HASH_SIZE;
  uns h4 = hash % HASH_SIZE;
  struct hash_record *rec = hash_rec + head;
  rec->string = string;
  rec->next3 = tab3[h3];
  rec->next4 = tab4[h4];
  tab3[h3] = head;				/* add the new record before the link-list */
  tab4[h4] = head;
  head++;					/* circular buffer, reuse old records */
  if (head >= HASH_RECORDS)
    head = 0;
  return head;
}

static inline byte *
dump_unary_value(byte *out, uns l)
{
  while (l > 255)
  {
    l -= 255;
    *out++ = 0;
  }
  *out++ = l;
  return out;
}

static byte *
flush_copy_command(uns bof, byte *out, byte *start, uns len)
{
  if (bof && len <= 238)
    *out++ = len + 17;
  else if (len < 4)
    /* cannot happen when !!bof */
    out[-2] |= len;				/* invariant: lowest 2 bits 2 bytes back */
  else
  {
    /* leave 2 least significant bits of out[-2] set to 0 */
    if (len <= 18)
      *out++ = len - 3;
    else
    {
      *out++ = 0;
      out = dump_unary_value(out, len - 18);
    }
  }
  while (len-- > 0)
    *out++ = *start++;
  return out;
}

int
lizzard_compress(byte *in, uns in_len, byte *out)
  /* Requires out being allocated for at least in_len * LIZZARD_MAX_PROLONG_FACTOR.
   * There must be at least 8 characters allocated after in.
   * Returns the actual compressed length. */
{
  hash_ptr_t hash_tab3[HASH_SIZE], hash_tab4[HASH_SIZE];
  struct hash_record hash_rec[HASH_RECORDS];
  byte *in_start = in;
  byte *in_end = in + in_len;
  byte *out_start = out;
  byte *copy_start = in;
  uns head = 0;
  bzero(hash_tab3, sizeof(hash_tab3));		/* init the hash-table */
  bzero(hash_tab4, sizeof(hash_tab4));
  while (in < in_end)
  {
    uns hash = hashf(in);
    byte *best;
    uns len = find_match(4, hash_tab4, hash_rec, hash, in, in_end, &best);
    if (len >= 3)
      goto match;
    len = find_match(3, hash_tab3, hash_rec, hash, in, in_end, &best);

match_found:
    if (len >= 3)
      goto match;
#if 0			// TODO: now, our routine does not detect matches of length 2
    if (len == 2 && (in - best->string) < ((1<<10) + 1))
      goto match;
#endif
literal:
    head = hash_string(in, hash, hash_tab3, hash_tab4, hash_rec, head);
    in++;					/* add a literal */
    continue;

match:
    if (in + len > in_end)			/* crop EOF */
    {
      len = in_end - in;
      if (len < 3)
	goto match_found;
    }
    /* Record the match.  */
    uns copy_len = in - copy_start;
    uns is_in_copy_mode = copy_start==in_start || copy_len >= 4;
    uns shift = in - best - 1;
    /* Try to use a 2-byte sequence.  */
    if (len == 2)
    {
      if (is_in_copy_mode || !copy_len)		/* cannot use with 0 copied characters, because this bit pattern is reserved for copy mode */
	goto literal;

dump_2sequence:
      if (copy_len > 0)
	out = flush_copy_command(copy_start==in_start, out, copy_start, copy_len);
      *out++ = (shift>>6) & ~3;			/* shift fits into 10 bits */
      *out++ = shift & 0xff;
    }
    else if (len == 3 && is_in_copy_mode && shift >= (1<<11) && shift < (1<<11 + 1<<10))
    {
      /* optimisation for length-3 matches after a copy command */
      shift -= 1<<11;
      goto dump_2sequence;
    }
    /* now, len >= 3 */
    else if (shift < (1<<11) && len <= 8)
    {
      shift |= (len-3 + 2)<<11;
      goto dump_2sequence;			/* shift has 11 bits and contains also len */
    }
    /* We have to use a 3-byte sequence.  */
    else if (len == 3 && is_in_copy_mode)
      /* avoid 3-sequence compressed to 3 sequence if it can simply be appended */
      goto literal;
    else
    {
      if (copy_len > 0)
	out = flush_copy_command(copy_start==in_start, out, copy_start, copy_len);
      if (shift < (1<<14))
      {
	if (len <= 33)
	  *out++ = (1<<5) | (len-2);
	else
	{
	  *out++ = 1<<5;
	  out = dump_unary_value(out, len - 33);
	}
      }
      else /* shift < (1<<15)-1 becase of HASH_RECORDS */
      {
	shift++;				/* because shift==0 is reserved for EOF */
	byte pos_bit = (shift>>11) & (1<<3);
	if (len <= 9)
	  *out++ = (1<<4) | pos_bit | (len-2);
	else
	{
	  *out++ = (1<<4) | pos_bit;
	  out = dump_unary_value(out, len - 9);
	}
      }
      *out++ = (shift>>6) & ~3;			/* rest of shift fits into 14 bits */
      *out++ = shift & 0xff;
    }
    /* Update the hash-table.  */
    head = hash_string(in, hash, hash_tab3, hash_tab4, hash_rec, head);
    for (uns i=1; i<len; i++)
      head = hash_string(in+i, hashf(in+i), hash_tab3, hash_tab4, hash_rec, head);
    in += len;
    copy_start = in;
  }
  if (in > in_end)				/* crop at BOF */
    in = in_end;
  uns copy_len = in - copy_start;
  if (copy_len > 0)
    out = flush_copy_command(copy_start==in_start, out, copy_start, copy_len);
  *out++ = 17;					/* add EOF */
  *out++ = 0;
  *out++ = 0;
  return out - out_start;
}

static inline byte *
read_unary_value(byte *in, uns *val)
{
  uns l = 0;
  while (!*in++)
    l += 255;
  l += in[-1];
  *val = l;
  return in;
}

int
lizzard_decompress(byte *in, byte *out)
  /* Requires out being allocated for the decompressed length must be known
   * beforehand.  It is desirable to lock the following memory page for
   * read-only access to prevent buffer overflow.  Returns the actual
   * decompressed length or a negative number when an error has occured.  */
{
  byte *out_start = out;
  uns expect_copy_command = 1;
  uns len;
  if (*in > 17)					/* short copy command at BOF */
  {
    len = *in++ - 17;
    expect_copy_command = 2;
    goto perform_copy_command;
  }
  while (1)
  {
    uns c = *in++;
    uns pos;
    if (c < 0x10)
      if (expect_copy_command == 1)
      {
	if (!c)
	  in = read_unary_value(in, &len);
	else
	  len = c;
	expect_copy_command = 2;
	goto perform_copy_command;
      }
      else
      {
	pos = (c&0xc)<<6 | *in++;
	if (expect_copy_command == 2)
	{
	  pos += 1<<11;
	  len = 3;
	}
	else
	  len = 2;
      }
    else if (c < 0x20)
    {
      pos = (c&0x8)<<11;
      len = c&0x7;
      if (len)
      {
	in = read_unary_value(in, &len);
	len += 9;
      }
      else
	len += 2;
      pos |= (*in++ & 0xfc)<<6;
      pos |= *in++;
      if (!pos)					/* EOF */
	break;
      else					/* shift it back */
	pos--;
    }
    else if (c < 0x40)
    {
      len = c&0x1f;
      if (len)
      {
	in = read_unary_value(in, &len);
	len += 33;
      }
      else
	len += 2;
      pos = (*in++ & 0xfc)<<6;
      pos |= *in++;
    }
    else /* high bits encode the length */
    {
      len = (c&0xe)>>5 -2 +3;
      pos = (c&0x1c)<<6;
      pos |= *in++;
    }
    /* take from the sliding window */
    memcpy(out, in-1-pos, len);			//FIXME: overlapping
    out += len;
    /* extract the copy-bits */
    len = in[-2] & 0x3;
    if (len)
      expect_copy_command = 0;			/* and fall-thru */
    else
    {
      expect_copy_command = 1;
      continue;
    }

perform_copy_command:
    memcpy(out, in, len);
    out += len;
    in += len;
  }

  return out - out_start;
}

/*

Description of the LZO1X format :
=================================

Beginning of file:
------------------

If the first byte is 00010001, it means probably EOF (empty file), so switch
to the compressed mode.  If it is bigger, subtract 17 and copy this number of
the following characters to the ouput and switch to the compressed mode.  If
it is smaller, go to the copy mode.

Compressed mode :
-----------------

Read the first byte of the sequence and determine the type of bit-encoding by
looking at the most significant bits.  The sequence is always at least 2 bytes
long.  Decode sequences of these types until the EOF or END marker is read.

  length L = length of the text taken from the sliding window

    If L=0, then count the number Z of the following zero bytes and add Z*255
    to the value of the following non-zero byte.  This allows setting L
    arbitrarily high.

  position p = relative position of the beginning of the text

    Exception: 00010001 00000000 00000000 means EOF

  copying C = length 1..3 of copied characters or END=0

    C following characters will be copied from the compressed text to the
    output.  The number CC is always stored in the 2 least significant bits of
    the second last byte of the sequence.
    
    If END is read, the algorithm switches to the copy mode.

pattern					length		position

0000ppCC 		 pppppppp	2		10 bits (*)
0001pLLL L*	ppppppCC pppppppp	3..9 + extend	15 bits + EOF
001LLLLL L*	ppppppCC pppppppp	3..33 + extend	14 bits
01\
10 \
11  \
LLLpppCC		 pppppppp	3..8		11 bits

Copy mode :
-----------

Read the first byte and, if the most significant bits are 0000, perform the
following command, otherwise switch to the compressed mode (and evaluate the
command).

pattern					length		position

0000LLLL L*				4..18 + extend	N/A

  Copy L characters from the compressed text to the output.  The overhead for
  incompressible strings is only roughly 1/256 + epsilon.

(*) After reading one copy command, switch to the compressed mode with the
following "optimisation": the pattern 0000ppCC expands to length 3 instead of 2
and 2048 is added to the position (now it is slightly more than 11 bits),
because a sequence of length 2 would never be used.

*/
