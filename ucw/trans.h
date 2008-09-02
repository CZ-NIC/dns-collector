/*
 *	The UCW Library -- Transactions
 *
 *	(c) 2008 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_TRANS_H
#define _UCW_TRANS_H

#include "ucw/mempool.h"

#include <setjmp.h>

/* Transactions */

struct trans {
  struct trans *prev_trans;
  struct mempool_state *trans_pool_state;
  struct respool *rpool;
  struct respool *prev_rpool;
  jmp_buf jmp;
};

void trans_init(void);		// Called automatically on trans_open() if needed
void trans_cleanup(void);	// Free memory occupied by the transaction system pools

struct trans *trans_open_rp(struct respool *rp);
static inline struct trans *trans_open(void)
{
  return trans_open_rp(NULL);
}
struct trans *trans_get_current(void);
void trans_commit(void);
void trans_rollback(void);
void trans_dump(void);

/* Exceptions */

#endif
