/*
 *	UCW Library -- Generic Binary Search
 *
 *	(c) 2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define BIN_SEARCH_FIRST_GE_CMP(ary,N,x,ary_lt_x)  ({		\
  uns l = 0, r = (N);						\
  while (l < r)							\
    {								\
      uns m = (l+r)/2;						\
      if (ary_lt_x(ary,m,x))					\
	l = m+1;						\
      else							\
	r = m;							\
    }								\
  l;								\
})

#define ARY_LT_NUM(ary,i,x) (ary)[i] < (x)

#define BIN_SEARCH_FIRST_GE(ary,N,x) BIN_SEARCH_FIRST_GE_CMP(ary,N,x,ARY_LT_NUM)
#define BIN_SEARCH_EQ(ary,N,x) ({ int i = BIN_SEARCH_FIRST_GE(ary,N,x); if (i >= (N) || (ary)[i] != (x)) i=-1; i; })
