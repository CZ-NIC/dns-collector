/*
 *	UCW Library -- Universal Sorter: Multi-Way Merge Module
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

static inline void P(update_tree)(P(key) *keys, int *tree, uns i)
{
  while (i /= 2)
    {
      if (tree[2*i] < 0)
	tree[i] = tree[2*i+1];
      else if (tree[2*i+1] < 0)
	tree[i] = tree[2*i];
      else if (P(compare)(&keys[tree[2*i]], &keys[tree[2*i+1]]) <= 0)
	tree[i] = tree[2*i];
      else
	tree[i] = tree[2*i+1];
    }
}

static void P(multiway_merge)(struct sort_context *ctx UNUSED, struct sort_bucket **ins, uns num_ins, struct sort_bucket *out)
{
  uns n2 = 1;
  while (n2 < num_ins)
    n2 *= 2;

  struct fastbuf *fout = sbuck_write(out);
  struct fastbuf *fins[num_ins];
  P(key) keys[num_ins];		// FIXME: Tune num_ins according to L1 cache size
  int tree[2*n2];		// A complete binary tree, leaves are input streams, each internal vertex contains a minimum of its sons
  for (uns i=1; i<=n2; i++)
    tree[i] = -1;

  for (uns i=0; i<num_ins; i++)
    {
      fins[i] = sbuck_read(ins[i]);
      if (P(read_key)(fins[i], &keys[i]))
	{
	  tree[n2+i] = i;
	  P(update_tree)(keys, tree, n2+i);
	}
    }

  while (tree[1] >= 0)
    {
      uns i = tree[1];
      P(copy_data)(&keys[i], fins[i], fout);
      if (unlikely(!P(read_key)(fins[i], &keys[i])))
	tree[n2+i] = -1;
      P(update_tree)(keys, tree, n2+i);
    }

#if 0
#ifdef SORT_ASSERT_UNIQUE
  ASSERT(comp != 0);
#endif
#ifdef SORT_UNIFY
  if (comp == 0)
    {
      P(key) *mkeys[] = { kin1, kin2 };
      struct fastbuf *mfb[] = { fin1, fin2 };
      P(copy_merged)(mkeys, mfb, 2, fout1);
      SWAP(kin1, kprev1, ktmp);
      next1 = P(read_key)(fin1, kin1);
      run1 = next1 && (P(compare)(kprev1, kin1) LESS 0);
      SWAP(kin2, kprev2, ktmp);
      next2 = P(read_key)(fin2, kin2);
      run2 = next2 && (P(compare)(kprev2, kin2) LESS 0);
      kout = kprev2;
    }
#endif
#endif

  out->runs++;
}
