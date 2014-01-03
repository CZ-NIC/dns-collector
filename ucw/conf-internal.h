/*
 *	UCW Library -- Configuration files: only for internal use of conf-*.c
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef	_UCW_CONF_INTERNAL_H
#define	_UCW_CONF_INTERNAL_H

#include <ucw/threads.h>

/* Item stack used by conf-intr.c */

#define MAX_STACK_SIZE 16

struct item_stack {		// used by conf-intr.c
  struct cf_section *sec;	// nested section
  void *base_ptr;		// because original pointers are often relative
  int op;			// it is performed when a closing brace is encountered
  void *list;			// list the operations should be done on
  u32 mask;			// bit array of selectors searching in a list
  struct cf_item *item;		// cf_item of the list
};

/* List of dirty sections used by conf-section.c */

struct dirty_section {
  struct cf_section *sec;
  void *ptr;
};

#define GBUF_TYPE	struct dirty_section
#define GBUF_PREFIX(x)	dirtsec_##x
#include <ucw/gbuf.h>

/* Configuration context */

struct cf_context {
  struct mempool *pool;
  int is_active;
  int config_loaded;			// at least one config file was loaded
  struct cf_parser_state *parser;
  uns everything_committed;		// did we already commit each section?
  uns postpone_commit;			// counter of calls to cf_open_group()
  uns other_options;			// used internally by cf_getopt()
  clist conf_entries;			// files/strings to reload
  struct cf_journal_item *journal;	// journalling
  int enable_journal;
  struct old_pools *pools;
  struct item_stack stack[MAX_STACK_SIZE];	// interpreter stack
  uns stack_level;
  struct cf_section sections;		// root section
  uns sections_initialized;
  dirtsec_t dirty;			// dirty sections
  uns dirties;
};

/* conf-ctxt.c */
static inline struct cf_context *cf_get_context(void)
{
  struct cf_context *cc = ucwlib_thread_context()->cf_context;
  ASSERT(cc->is_active);
  return cc;
}

// In fact, this is equivalent to cf_get_context(), but it is not inlined,
// because we want to force the linker to include conf-context.c, which contains
// a constructor of the whole context mechanism.
struct cf_context *cf_obtain_context(void);

/* conf-intr.c */
#define OP_MASK 0xff		// only get the operation
#define OP_OPEN 0x100		// here we only get an opening brace instead of parameters
#define OP_1ST 0x200		// in the 1st phase selectors are recorded into the mask
#define OP_2ND 0x400		// in the 2nd phase real data are entered
enum cf_operation;
extern char *cf_op_names[];
extern char *cf_type_names[];

uns cf_type_size(enum cf_type type, struct cf_user_type *utype);
char *cf_interpret_line(struct cf_context *cc, char *name, enum cf_operation op, int number, char **pars);
void cf_init_stack(struct cf_context *cc);
int cf_done_stack(struct cf_context *cc);

/* conf-journal.c */
void cf_journal_swap(void);
void cf_journal_delete(void);

/* conf-section.c */
#define SEC_FLAG_DYNAMIC	0x80000000	// contains a dynamic attribute
#define SEC_FLAG_UNKNOWN	0x40000000	// ignore unknown entriies
#define SEC_FLAG_CANT_COPY	0x20000000	// contains lists or parsers
#define SEC_FLAG_NUMBER		0x0fffffff	// number of entries
enum cf_commit_mode { CF_NO_COMMIT, CF_COMMIT, CF_COMMIT_ALL };
extern struct cf_section cf_sections;

struct cf_item *cf_find_subitem(struct cf_section *sec, const char *name);
int cf_commit_all(enum cf_commit_mode cm);
void cf_add_dirty(struct cf_section *sec, void *ptr);

#endif
