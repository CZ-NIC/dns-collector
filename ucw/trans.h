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

struct trans *trans_open(void);
struct trans *trans_get_current(void);
void trans_commit(void);
void trans_rollback(void);
void trans_dump(void);

struct mempool *trans_get_pool(void);
struct mempool *trans_get_exc_pool(void);

/* Exceptions */

struct exception {
  const char *id;		// Hierarchic identifier of the exception
  const char *msg;		// Error message to present to the user
  void *object;			// Object on which the exception happened
  struct trans *trans;		// Transaction in which it happened (set by trans_throw*)
  // More data specific for the particular `id' can follow
};

void trans_throw_exc(struct exception *x) NONRET;
void trans_throw(const char *id, void *object, const char *fmt, ...) FORMAT_CHECK(printf,3,4) NONRET;
void trans_vthrow(const char *id, void *object, const char *fmt, va_list args) NONRET;

struct exception *trans_current_exc(void);

#define TRANS_TRY do {				\
  struct trans *_t = trans_open();		\
  if (!setjmp(_t->jmp))				\
    {

#define TRANS_CATCH(x)				\
      trans_commit();				\
    }						\
  else						\
    {						\
      struct exception *x UNUSED = trans_current_exc();

#define TRANS_END				\
      trans_rollback();				\
    }						\
  } while(0)

#endif
