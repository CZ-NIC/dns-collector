/*
 *	The UCW Library -- Transactions
 *
 *	(c) 2008--2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_TRANS_H
#define _UCW_TRANS_H

#include <ucw/mempool.h>

#include <setjmp.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define trans_caught ucw_trans_caught
#define trans_cleanup ucw_trans_cleanup
#define trans_commit ucw_trans_commit
#define trans_current_exc ucw_trans_current_exc
#define trans_dump ucw_trans_dump
#define trans_fold ucw_trans_fold
#define trans_get_current ucw_trans_get_current
#define trans_get_pool ucw_trans_get_pool
#define trans_init ucw_trans_init
#define trans_open ucw_trans_open
#define trans_rollback ucw_trans_rollback
#define trans_throw ucw_trans_throw
#define trans_throw_exc ucw_trans_throw_exc
#define trans_vthrow ucw_trans_vthrow
#endif

/** A structure describing a transaction. All fields are for internal use only. **/
struct trans {
  struct trans *prev_trans;
  struct mempool_state *trans_pool_state;
  struct respool *rpool;
  struct respool *prev_rpool;
  struct exception *thrown_exc;
  jmp_buf jmp;
};

void trans_init(void);		/** Initializes the transaction system for the current thread. Called automatically as needed. **/
void trans_cleanup(void);	/** Frees memory occupied by the transaction system pools for the current thread. **/

struct trans *trans_open(void);	/** Creates a new transaction. Used inside `TRANS_TRY`. **/
struct trans *trans_get_current(void);	/** Get a pointer to the currently running transaction, or NULL if there is none. **/
void trans_commit(void);	/** Commits the current transaction. **/
void trans_rollback(void);	/** Rolls back the current transaction. **/
void trans_fold(void);		/** Folds the current transaction to its parent. **/
void trans_dump(void);		/** Prints out a debugging dump of the transaction stack to stdout. **/

struct mempool *trans_get_pool(void);

/**
 * Data associated with an exception. Usually, this structure is created
 * by calling @trans_throw(), but if you want to pass more data, you can
 * create your own exception and throw it using @trans_throw_exc().
 **/
struct exception {
  const char *id;		// Hierarchical identifier of the exception
  const char *msg;		// Error message to present to the user
  void *object;			// Object on which the exception happened
  // More data specific for the particular `id' can follow
};

/** Creates an exception and throws it. The error message can contain `printf`-like formatting. **/
void trans_throw(const char *id, void *object, const char *fmt, ...) FORMAT_CHECK(printf,3,4) NONRET;

/** A `va_list` variant of @trans_throw(). **/
void trans_vthrow(const char *id, void *object, const char *fmt, va_list args) NONRET;

/** Throw an already constructed exception (or re-throw an exception you have caught). **/
void trans_throw_exc(struct exception *x) NONRET;

/** Declare the current exception caught and roll back the current transaction. Called from `TRANS_END`. **/
void trans_caught(void);

struct exception *trans_current_exc(void);	/** Return the exception in flight, or NULL if there is none. **/

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
      trans_caught();				\
    }						\
  } while(0)

#endif
