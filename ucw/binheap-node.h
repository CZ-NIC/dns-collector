/*
 *	UCW Library -- Binomial Heaps: Declarations
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

struct bh_node {
  struct bh_node *first_son;
  struct bh_node *last_son;
  struct bh_node *next_sibling;
  byte order;
};

struct bh_heap {
  struct bh_node root;
};
