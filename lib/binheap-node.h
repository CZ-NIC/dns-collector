/*
 *	Sherlock Library -- Binomial Heaps: Declarations
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define BH_NODE struct BH_PREFIX(node)
#define BH_HEAP struct BH_PREFIX(heap)

BH_NODE {
  BH_NODE *first_son;
  BH_NODE *last_son;
  BH_NODE *next_sibling;
  byte order;
};

BH_HEAP {
  BH_NODE root;
};
