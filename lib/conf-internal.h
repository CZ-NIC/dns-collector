/* conf-intr.c */
#define OP_MASK 0xff		// only get the operation
#define OP_OPEN 0x100		// here we only get an opening brace instead of parameters
#define OP_1ST 0x200		// in the 1st phase selectors are recorded into the mask
#define OP_2ND 0x400		// in the 2nd phase real data are entered
enum cf_operation;
extern byte *cf_op_names[];

uns cf_type_size(enum cf_type type, struct cf_user_type *utype);
byte *cf_interpret_line(byte *name, enum cf_operation op, int number, byte **pars);
void cf_init_stack(void);
int cf_check_stack(void);

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

struct cf_item *cf_find_subitem(struct cf_section *sec, byte *name);
int cf_commit_all(enum cf_commit_mode cm);
void cf_add_dirty(struct cf_section *sec, void *ptr);
