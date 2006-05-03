/*
 *	Image Library -- multidimensional Hilbert curves
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 *
 *
 *	References:
 *	- http://www.dcs.bbk.ac.uk/~jkl/mapping.c
 *	  (c) 2002 J.K.Lawder
 *	- J.K. Lawder. Calculation of Mappings between One and n-dimensional Values
 *	  Using the Hilbert Space-Filling Curve. Technical Report JL1/00, Birkbeck
 *	  College, University of London, 2000.
 *
 *	FIXME:
 *	- the algorithm fails for some combinations of HILBERT_DIM and HILBERT_ORDER,
 *	  but it should be safe for HILBERT_DIM = 2..8, HILBERT_ORDER = 8..32
 *	- clean and optimize the code
 */

#ifndef HILBERT_PREFIX
#  error Undefined HILBERT_PREFIX
#endif

#define P(x) HILBERT_PREFIX(x)

/*
 * HILBERT_DIM is the number of dimensions in space through which the
 * Hilbert Curve passes.
 * Don't use this implementation with values for HILBERT_DIM of > 31!
 * Also, make sure you use a 32 bit compiler!
 */
#ifndef HILBERT_DIM
#  define HILBERT_DIM 2
#endif

#ifndef HILBERT_TYPE
#  define HILBERT_TYPE u32
#endif

#ifndef HILBERT_ORDER
#  define HILBERT_ORDER (8 * sizeof(HILBERT_TYPE))
#endif

typedef HILBERT_TYPE P(t);

/*
 * retained for historical reasons: the number of bits in an attribute value:
 * effectively the order of a curve
 */
#define NUMBITS HILBERT_ORDER

/*
 * the number of bits in a word used to store an hcode (or in an element of
 * an array that's used)
 */
#define WORDBITS HILBERT_ORDER

#ifdef HILBERT_WANT_ENCODE
/*
 * given the coordinates of a point, it finds the sequence number of the point
 * on the Hilbert Curve
 */
static void
P(encode) (P(t) *dest, P(t) *src)
{
  P(t) mask = (P(t))1 << WORDBITS - 1, element, temp1, temp2,
    A, W = 0, S, tS, T, tT, J, P = 0, xJ;
  uns i = NUMBITS * HILBERT_DIM - HILBERT_DIM, j;

  for (j = 0; j < HILBERT_DIM; j++)
    dest[j] = 0;
  for (j = A = 0; j < HILBERT_DIM; j++)
    if (src[j] & mask)
      A |= (1 << HILBERT_DIM - 1 - j);

  S = tS = A;

  P |= S & (1 << HILBERT_DIM - 1);
  for (j = 1; j < HILBERT_DIM; j++)
    if( S & (1 << HILBERT_DIM - 1 - j) ^ (P >> 1) & (1 << HILBERT_DIM - 1 - j))
      P |= (1 << HILBERT_DIM - 1 - j);

  /* add in HILBERT_DIM bits to hcode */
  element = i / WORDBITS;
  if (i % WORDBITS > WORDBITS - HILBERT_DIM)
    {
      dest[element] |= P << i % WORDBITS;
      dest[element + 1] |= P >> WORDBITS - i % WORDBITS;
    }
  else
    dest[element] |= P << i - element * WORDBITS;

  J = HILBERT_DIM;
  for (j = 1; j < HILBERT_DIM; j++)
    if ((P >> j & 1) == (P & 1))
      continue;
    else
      break;
  if (j != HILBERT_DIM)
    J -= j;
  xJ = J - 1;

  if (P < 3)
    T = 0;
  else
    if (P % 2)
      T = (P - 1) ^ (P - 1) / 2;
    else
      T = (P - 2) ^ (P - 2) / 2;
  tT = T;

  for (i -= HILBERT_DIM, mask >>= 1; (int)i >= 0; i -= HILBERT_DIM, mask >>= 1)
    {
      for (j = A = 0; j < HILBERT_DIM; j++)
        if (src[j] & mask)
          A |= (1 << HILBERT_DIM - 1 - j);

      W ^= tT;
      tS = A ^ W;
      if (xJ % HILBERT_DIM != 0)
        {
          temp1 = tS << xJ % HILBERT_DIM;
          temp2 = tS >> HILBERT_DIM - xJ % HILBERT_DIM;
	  S = temp1 | temp2;
	  S &= ((P(t))1 << HILBERT_DIM) - 1;
	}
      else
	S = tS;

      P = S & (1 << HILBERT_DIM - 1);
      for (j = 1; j < HILBERT_DIM; j++)
	if( S & (1 << HILBERT_DIM - 1 - j) ^ (P >> 1) & (1 << HILBERT_DIM - 1 - j))
	  P |= (1 << HILBERT_DIM - 1 - j);

      /* add in HILBERT_DIM bits to hcode */
      element = i / WORDBITS;
      if (i % WORDBITS > WORDBITS - HILBERT_DIM)
	{
	  dest[element] |= P << i % WORDBITS;
	  dest[element + 1] |= P >> WORDBITS - i % WORDBITS;
	}
      else
	dest[element] |= P << i - element * WORDBITS;

      if (i > 0)
	{
	  if (P < 3)
	    T = 0;
	  else
	    if (P % 2)
	      T = (P - 1) ^ (P - 1) / 2;
	    else
	      T = (P - 2) ^ (P - 2) / 2;

	  if (xJ % HILBERT_DIM != 0)
	    {
	      temp1 = T >> xJ % HILBERT_DIM;
	      temp2 = T << HILBERT_DIM - xJ % HILBERT_DIM;
	      tT = temp1 | temp2;
	      tT &= ((P(t))1 << HILBERT_DIM) - 1;
	    }
	  else
	    tT = T;

	  J = HILBERT_DIM;
	  for (j = 1; j < HILBERT_DIM; j++)
	    if ((P >> j & 1) == (P & 1))
	      continue;
	    else
	      break;
	  if (j != HILBERT_DIM)
	    J -= j;

	  xJ += J - 1;
          /* J %= HILBERT_DIM; */
	}
    }
  for (j = 0; j < HILBERT_DIM; j++)
    dest[j] &= ~(P(t))0 >> (8 * sizeof(P(t)) - WORDBITS);
}
#endif

#ifdef HILBERT_WANT_DECODE
/*
 * given the sequence number of a point, it finds the coordinates of the point
 * on the Hilbert Curve
 */
static void
P(decode) (P(t) *dest, P(t) *src)
{
  P(t) mask = (P(t))1 << WORDBITS - 1, element, temp1, temp2,
    A, W = 0, S, tS, T, tT, J, P = 0, xJ;
  uns i = NUMBITS * HILBERT_DIM - HILBERT_DIM, j;

  for (j = 0; j < HILBERT_DIM; j++)
    dest[j] = 0;

  /*--- P ---*/
  element = i / WORDBITS;
  P = src[element];
  if (i % WORDBITS > WORDBITS - HILBERT_DIM)
    {
      temp1 = src[element + 1];
      P >>= i % WORDBITS;
      temp1 <<= WORDBITS - i % WORDBITS;
      P |= temp1;
    }
  else
    P >>= i % WORDBITS;	/* P is a HILBERT_DIM bit hcode */

  /* the & masks out spurious highbit values */
  if (HILBERT_DIM < WORDBITS)
    P &= (1 << HILBERT_DIM) -1;

  /*--- xJ ---*/
  J = HILBERT_DIM;
  for (j = 1; j < HILBERT_DIM; j++)
    if ((P >> j & 1) == (P & 1))
      continue;
    else
      break;
  if (j != HILBERT_DIM)
    J -= j;
  xJ = J - 1;

  /*--- S, tS, A ---*/
  A = S = tS = P ^ P / 2;


  /*--- T ---*/
  if (P < 3)
    T = 0;
  else
    if (P % 2)
      T = (P - 1) ^ (P - 1) / 2;
    else
      T = (P - 2) ^ (P - 2) / 2;

  /*--- tT ---*/
  tT = T;

  /*--- distrib bits to coords ---*/
  for (j = HILBERT_DIM - 1; P > 0; P >>=1, j--)
    if (P & 1)
      dest[j] |= mask;


  for (i -= HILBERT_DIM, mask >>= 1; (int)i >= 0; i -= HILBERT_DIM, mask >>= 1)
    {
      /*--- P ---*/
      element = i / WORDBITS;
      P = src[element];
      if (i % WORDBITS > WORDBITS - HILBERT_DIM)
	{
	  temp1 = src[element + 1];
	  P >>= i % WORDBITS;
	  temp1 <<= WORDBITS - i % WORDBITS;
	  P |= temp1;
	}
      else
	P >>= i % WORDBITS;	/* P is a HILBERT_DIM bit hcode */

      /* the & masks out spurious highbit values */
      if (HILBERT_DIM < WORDBITS)
        P &= (1 << HILBERT_DIM) -1;

      /*--- S ---*/
      S = P ^ P / 2;

      /*--- tS ---*/
      if (xJ % HILBERT_DIM != 0)
	{
	  temp1 = S >> xJ % HILBERT_DIM;
	  temp2 = S << HILBERT_DIM - xJ % HILBERT_DIM;
	  tS = temp1 | temp2;
	  tS &= ((P(t))1 << HILBERT_DIM) - 1;
	}
      else
	tS = S;

      /*--- W ---*/
      W ^= tT;

      /*--- A ---*/
      A = W ^ tS;

      /*--- distrib bits to coords ---*/
      for (j = HILBERT_DIM - 1; A > 0; A >>=1, j--)
	if (A & 1)
	  dest[j] |= mask;

      if (i > 0)
	{
	  /*--- T ---*/
	  if (P < 3)
	    T = 0;
	  else
	    if (P % 2)
	      T = (P - 1) ^ (P - 1) / 2;
	    else
	      T = (P - 2) ^ (P - 2) / 2;

	  /*--- tT ---*/
	  if (xJ % HILBERT_DIM != 0)
	    {
	      temp1 = T >> xJ % HILBERT_DIM;
	      temp2 = T << HILBERT_DIM - xJ % HILBERT_DIM;
	      tT = temp1 | temp2;
	      tT &= ((P(t))1 << HILBERT_DIM) - 1;
	    }
	  else
	    tT = T;

	  /*--- xJ ---*/
	  J = HILBERT_DIM;
	  for (j = 1; j < HILBERT_DIM; j++)
	    if ((P >> j & 1) == (P & 1))
	      continue;
	    else
	      break;
	  if (j != HILBERT_DIM)
	    J -= j;
	  xJ += J - 1;
	}
    }
}
#endif

#undef P
#undef HILBERT_PREFIX
#undef HILBERT_DIM
#undef HILBERT_TYPE
#undef HILBERT_ORDER
#undef HILBERT_WANT_DECODE
#undef HILBERT_WANT_ENCODE
#undef NUMBITS
#undef WORDBITS
