/*
 *	LiZaRd -- Fast compression method based on Lempel-Ziv 77
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 *
 *	The file format is based on LZO1X and 
 *	the compression method is based on zlib.
 */

#include "lib/lib.h"
#include "lib/lizard.h"

#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <fcntl.h>
#include <signal.h>

typedef u16 hash_ptr_t;
struct hash_record {
  /* the position in the original text is implicit; it is computed by locate_string() */
  hash_ptr_t next;			// 0=end
  hash_ptr_t prev;			// high bit: 0=record in array, 1=head in hash-table (i.e. value of hashf)
};

#define	HASH_SIZE	(1<<14)		// size of hash-table
#define	HASH_RECORDS	(1<<15)		// maximum number of records in hash-table, 0 is unused ==> subtract 1
#define	CHAIN_MAX_TESTS		8	// crop longer collision chains
#define	CHAIN_GOOD_MATCH	32	// we already have a good match => end

static inline uns
hashf(byte *string)
  /* 0..HASH_SIZE-1 */
{
    return string[0] ^ (string[1]<<3) ^ (string[2]<<6);
}

static inline byte *
locate_string(byte *string, uns record_id, uns head)
  /* The strings are recorded into the hash-table regularly, hence there is no
   * need to store the pointer there.  */
{
  string += record_id - head;
  if (record_id >= head)
    string -= HASH_RECORDS-1;
  return string;
}

static inline uns
find_match(uns record_id, struct hash_record *hash_rec, byte *string, byte *string_end, byte **best_ptr, uns head)
  /* hash_tab[hash] == record_id points to the head of the double-linked
   * link-list of strings with the same hash.  The records are statically
   * stored in circular array hash_rec (with the 1st entry unused), and the
   * pointers are just 16-bit indices.  The strings in every collision chain
   * are ordered by age.  */
{
  uns count = CHAIN_MAX_TESTS;
  uns best_len = 0;
  while (record_id && count-- > 0)
  {
    byte *record_string = locate_string(string, record_id, head);
    byte *cmp = record_string;
    if (cmp[0] == string[0] && cmp[2] == string[2])
    /* implies cmp[1] == string[1] */
    {
      if (cmp[3] == string[3])
      {
	cmp += 4;
	if (*cmp++ == string[4] && *cmp++ == string[5]
	    && *cmp++ == string[6] && *cmp++ == string[7])
	{
	  byte *str = string + 8;
	  while (str <= string_end && *cmp++ == *str++);
	}
      }
      else
	cmp += 4;
      uns len = cmp - record_string - 1;	/* cmp points 2 characters after the last match */
      if (len > best_len)
      {
	best_len = len;
	*best_ptr = record_string;
	if (best_len >= CHAIN_GOOD_MATCH)	/* optimization */
	  break;
      }
    }
    record_id = hash_rec[record_id].next;
  }
  return best_len;
}

static uns
hash_string(hash_ptr_t *hash_tab, uns hash, struct hash_record *hash_rec, /*byte *string,*/ uns head, uns *to_delete)
  /* We reuse hash-records stored in a circular array.  First, delete the old
   * one and then add the new one in front of the link-list.  */
{
  struct hash_record *rec = hash_rec + head;
  if (*to_delete)				/* unlink the original record */
  {
    uns prev_id = rec->prev & ((1<<15)-1);
    if (rec->prev & (1<<15))			/* was a head */
      hash_tab[prev_id] = 0;
    else					/* thanks to the ordering, this was a tail */
      hash_rec[prev_id].next = 0;
  }
  rec->next = hash_tab[hash];
  rec->prev = (1<<15) | hash;
  hash_rec[rec->next].prev = head;
  hash_tab[hash] = head;			/* add the new record before the link-list */

  if (++head >= HASH_RECORDS)			/* circular buffer, reuse old records, 0 is unused */
  {
    head = 1;
    *to_delete = 1;
  }
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
lizard_compress(byte *in, uns in_len, byte *out)
  /* Requires out being allocated for at least in_len * LIZARD_MAX_MULTIPLY +
   * LIZARD_MAX_ADD.  There must be at least LIZARD_NEEDS_CHARS characters
   * allocated after in.  Returns the actual compressed length. */
{
  hash_ptr_t hash_tab[HASH_SIZE];
  struct hash_record hash_rec[HASH_RECORDS];
  byte *in_start = in;
  byte *in_end = in + in_len;
  byte *out_start = out;
  byte *copy_start = in;
  uns head = 1;					/* 0 in unused */
  uns to_delete = 0;
  bzero(hash_tab, sizeof(hash_tab));		/* init the hash-table */
  while (in < in_end)
  {
    uns hash = hashf(in);
    byte *best;
    uns len = find_match(hash_tab[hash], hash_rec, in, in_end, &best, head);
    if (len < 3)
#if 0			// TODO: now, our routine does not detect matches of length 2
      if (len == 2 && (in - best->string) < ((1<<10) + 1))
      { /* pass-thru */ }
      else
#endif
      {
literal:
	head = hash_string(hash_tab, hash, hash_rec, head, &to_delete);
	in++;					/* add a literal */
	continue;
      }

    if (in + len > in_end)			/* crop EOF */
    {
      len = in_end - in;
      if (len < 3)
	goto literal;
    }
    /* Record the match.  */
    uns copy_len = in - copy_start;
    uns is_in_copy_mode = copy_start==in_start || copy_len >= 4;
    uns shift = in - best - 1;
    /* Try to use a 2-byte sequence.  */
#if 0
    if (len == 2)
    {
      if (is_in_copy_mode || !copy_len)		/* cannot use with 0 copied characters, because this bit pattern is reserved for copy mode */
	goto literal;
      else
	goto dump_2sequence;
    } else
#endif
    if (len == 3 && is_in_copy_mode && shift >= (1<<11) && shift < (1<<11) + (1<<10))
    {
      /* optimisation for length-3 matches after a copy command */
      shift -= 1<<11;
dump_2sequence:
      if (copy_len > 0)
	out = flush_copy_command(copy_start==in_start, out, copy_start, copy_len);
      *out++ = (shift>>6) & ~3;			/* shift fits into 10 bits */
      *out++ = shift & 0xff;
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
    head = hash_string(hash_tab, hash, hash_rec, head, &to_delete);
    for (uns i=1; i<len; i++)
      head = hash_string(hash_tab, hashf(in+i), hash_rec, head, &to_delete);
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
lizard_decompress(byte *in, byte *out)
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
	{
	  in = read_unary_value(in, &len);
	  len += 18;
	}
	else
	  len = c + 3;
	expect_copy_command = 2;
	goto perform_copy_command;
      }
      else
      {
	pos = ((c&0xc)<<6) | *in++;
	if (expect_copy_command == 2)
	{
	  pos += 1<<11;
	  len = 3;
	}
	else
	  len = 2;
	pos++;
      }
    else if (c < 0x20)
    {
      pos = (c&0x8)<<11;
      len = c&0x7;
      if (!len)
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
      /* do NOT pos++ */
    }
    else if (c < 0x40)
    {
      len = c&0x1f;
      if (!len)
      {
	in = read_unary_value(in, &len);
	len += 33;
      }
      else
	len += 2;
      pos = (*in++ & 0xfc)<<6;
      pos |= *in++;
      pos++;
    }
    else /* high bits encode the length */
    {
      len = ((c&0xe0)>>5) -2 +3;
      pos = (c&0x1c)<<6;
      pos |= *in++;
      pos++;
    }
    /* take from the sliding window */
    if (len <= pos)
    {
      memcpy(out, out-pos, len);
      out += len;
    }
    else
    {						/* overlapping */
      for (; len-- > 0; out++)
	*out = out[-pos];
    }
    /* extract the copy-bits */
    len = in[-2] & 0x3;
    if (len)
    {
      expect_copy_command = 0;
      while (len-- > 0)
	*out++ = *in++;
    }
    else
      expect_copy_command = 1;
    continue;

perform_copy_command:
    memcpy(out, in, len);
    out += len;
    in += len;
  }

  return out - out_start;
}

struct lizard_buffer *
lizard_alloc(uns max_len)
{
  int fd = open("/dev/zero", O_RDWR);
  if (fd < 0)
    die("open(/dev/zero): %m");
  struct lizard_buffer *buf = xmalloc(sizeof(struct lizard_buffer));
  buf->len = (max_len + 2*PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
  buf->ptr = mmap(NULL, buf->len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (buf->ptr == MAP_FAILED)
    die("mmap(/dev/zero): %m");
  return buf;
}

void
lizard_free(struct lizard_buffer *buf)
{
  munmap(buf->ptr, buf->len);
  xfree(buf);
}

static void
sigsegv_handler(int UNUSED whatsit)
{
  die("SIGSEGV caught when decompressing.");
}

int
lizard_decompress_safe(byte *in, struct lizard_buffer *buf, uns expected_length)
  /* Decompresses into buf->ptr and returns the length of the uncompressed
   * file.  Negative return values signalise errors.  If something goes wrong
   * and buffer would overflow, SIGSEGV is raised.  */
{
  uns lock_offset = (expected_length + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
  if (lock_offset + PAGE_SIZE > buf->len)
    return -1;
  mprotect(buf->ptr, lock_offset, PROT_READ | PROT_WRITE);
  mprotect(buf->ptr + lock_offset, PAGE_SIZE, PROT_READ);
  sighandler_t old_handler = signal(SIGSEGV, sigsegv_handler);
  int len = lizard_decompress(in, buf->ptr);
  signal(SIGSEGV, old_handler);
  return len;
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
