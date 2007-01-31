/*
 *	UCW Library -- Universal Sorter
 *
 *	(c) 2001--2007 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *  This is not a normal header file, but a generator of sorting
 *  routines.  Each time you include it with parameters set in the
 *  corresponding preprocessor macros, it generates a file sorter
 *  with the parameters given.
 *
 *  FIXME: explain the basics (keys and data, callbacks etc.)
 *
 *  Basic parameters and callbacks:
 *
 *  SORT_PREFIX(x)      add a name prefix (used on all global names
 *			defined by the sorter)
 *
 *  SORT_KEY		data type capable of storing a single key.
 *  SORT_KEY_REGULAR	data type holding a single key both in memory and on disk;
 *			in this case, bread() and bwrite() is used to read/write keys
 *			and it's also assumed that the keys are not very long.
 *  int PREFIX_compare(SORT_KEY *a, SORT_KEY *b)
 *			compares two keys, result like strcmp(). Mandatory.
 *  int PREFIX_read_key(struct fastbuf *f, SORT_KEY *k)
 *			reads a key from a fastbuf, returns nonzero=ok, 0=EOF.
 *			Mandatory unless SORT_KEY_REGULAR is defined.
 *  void PREFIX_write_key(struct fastbuf *f, SORT_KEY *k)
 *			writes a key to a fastbuf. Mandatory unless SORT_KEY_REGULAR.
 *
 *  SORT_KEY_SIZE(key)	returns the real size of a key (a SORT_KEY type in memory
 *			can be truncated to this number of bytes without any harm;
 *			used to save memory when the keys have variable sizes).
 *			Default: always store the whole SORT_KEY.
 *  SORT_DATA_SIZE(key)	gets a key and returns the amount of data following it.
 *			Default: records consist of keys only.
 *
 *  Integer sorting:
 *
 *  SORT_INT(key)	We are sorting by an integer value. In this mode, PREFIX_compare
 *			is supplied automatically and the sorting function gets an extra
 *			parameter specifying a range of the integers. The better the range
 *			fits, the faster we sort. Sets up SORT_HASH_xxx automatically.
 *
 *  Hashing (optional, but it can speed sorting up):
 *
 *  SORT_HASH_FN(key)	returns a monotone hash of a given key. Monotone hash is a function f
 *			such that f(x) < f(y) implies x < y and which is approximately uniformly
 *			distributed.
 *  SORT_HASH_BITS	how many bits do the hashes have.
 *
 *  Unification:
 *
 *  SORT_MERGE		merge items with identical keys, needs the following functions:
 *  void PREFIX_write_merged(struct fastbuf *f, SORT_KEY **keys, uns n, byte *buf)
 *			takes n records in memory with keys which compare equal and writes
 *			a single record to the given fastbuf. Data for each key can
 *			be accessed by the SORT_GET_DATA(*key) macro. `buf' points
 *			to a buffer which is guaranteed to hold all given records.
 *  void PREFIX_copy_merged(SORT_KEY **keys, struct fastbuf **data, uns n, struct fastbuf *dest)
 *			takes n records with keys in memory and data in fastbufs and writes
 *			a single record.
 *
 *  Input (choose one of these):
 *
 *  SORT_INPUT_FILE	file of a given name
 *  SORT_INPUT_FB	fastbuf stream
 *  SORT_INPUT_PRESORT	custom presorter: call function PREFIX_presorter (see below)
 *			to get successive batches of pre-sorted data as temporary
 *			fastbuf streams or NULL if no more data is available.
 *			The function is passed a page-aligned presorting buffer.
 *
 *  Output (chose one of these):
 *
 *  SORT_OUTPUT_FILE	file of a given name
 *  SORT_OUTPUT_FB	temporary fastbuf stream
 *  SORT_OUTPUT_THIS_FB	a given fastbuf stream which can already contain a header
 *
 *  FIXME: Maybe implement these:
 *  ??? SORT_UNIQUE		all items have distinct keys (checked in debug mode)
 *  ??? SORT_DELETE_INPUT	a C expression, if true, the input files are
 *			deleted as soon as possible
 *  ??? SORT_ALIGNED
 *
 *  The function generated:
 *
 *  <outfb> PREFIX_SORT(<in>, <out>, <range>), where:
 *			<in> = input file name/fastbuf
 *			<out> = output file name/fastbuf
 *			<range> = maximum integer value for the SORT_INT mode
 *			<outfb> = output fastbuf (in SORT_OUTPUT_FB mode)
 *			(any parameter can be missing if it is not applicable).
 *
 *  void PREFIX_merge_data(struct fastbuf *src1, *src2, *dest, SORT_KEY *k1, *k2)
 *			[used only in case SORT_UNIFY is defined]
 *			write just fetched key k to dest and merge data from
 *			two records with the same key (k1 and k2 are key occurences
 *			in the corresponding streams).
 *  SORT_KEY * PREFIX_merge_items(SORT_KEY *a, SORT_KEY *b)
 *			[used only with SORT_PRESORT && SORT_UNIFY]
 *			merge two items with the same key, returns pointer
 *			to at most one of the items, the rest will be removed
 *			from the list of items, but not deallocated, so
 *			the remaining item can freely reference data of the
 *			other one.
 *
 *  After including this file, all parameter macros are automatically
 *  undef'd.
 */

#define P(x) SORT_PREFIX(x)

#undef P
/* FIXME: Check that we undef everything we should. */
