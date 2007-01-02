/*
 *	The UCW Library -- Miscellaneous Functions
 *
 *	(c) 1997--2006 Martin Mares <mj@ucw.cz>
 *	(c) 2005 Tomas Valla <tom@ucw.cz>
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_LIB_H
#define _UCW_LIB_H

#include "lib/config.h"
#include <stdarg.h>

/* Tell libc we're going to use all extensions available */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Macros for handling structurues, offsets and alignment */

#define CHECK_PTR_TYPE(x, type) ((x)-(type)(x) + (type)(x))
#define PTR_TO(s, i) &((s*)0)->i
#define OFFSETOF(s, i) ((unsigned int) PTR_TO(s, i))
#define SKIP_BACK(s, i, p) ((s *)((char *)p - OFFSETOF(s, i)))
#define ALIGN_TO(s, a) (((s)+a-1)&~(a-1))
#define ALIGN_PTR(p, s) ((addr_int_t)(p) % (s) ? (typeof(p))((addr_int_t)(p) + (s) - (addr_int_t)(p) % (s)) : (p))
#define UNALIGNED_PART(ptr, type) (((addr_int_t) (ptr)) % sizeof(type))

/* Some other macros */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP(x,min,max) ({ int _t=x; (_t < min) ? min : (_t > max) ? max : _t; })
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))
#define STRINGIFY(x) #x
#define STRINGIFY_EXPANDED(x) STRINGIFY(x)
#define GLUE(x,y) x##y
#define GLUE_(x,y) x##_##y

#define COMPARE(x,y) do { if ((x)<(y)) return -1; if ((x)>(y)) return 1; } while(0)
#define REV_COMPARE(x,y) COMPARE(y,x)
#define COMPARE_LT(x,y) do { if ((x)<(y)) return 1; if ((x)>(y)) return 0; } while(0)
#define COMPARE_GT(x,y) COMPARE_LT(y,x)

#define	ROL(x, bits) (((x) << (bits)) | ((x) >> (sizeof(uns)*8 - (bits))))	/* Bitwise rotation of an uns to the left */

/* GCC Extensions */

#ifdef __GNUC__

#undef inline
#define NONRET __attribute__((noreturn))
#define UNUSED __attribute__((unused))
#define CONSTRUCTOR __attribute__((constructor))
#define PACKED __attribute__((packed))
#define CONST __attribute__((const))
#define PURE __attribute__((pure))
#define FORMAT_CHECK(x,y,z) __attribute__((format(x,y,z)))
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#if __GNUC__ >= 4 || __GNUC__ == 3 && __GNUC_MINOR__ >= 3
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#else
#define ALWAYS_INLINE inline
#endif

#if __GNUC__ >= 4
#define LIKE_MALLOC __attribute__((malloc))
#define SENTINEL_CHECK __attribute__((sentinel))
#else
#define LIKE_MALLOC
#define SENTINEL_CHECK
#endif

#else
#error This program requires the GNU C compiler.
#endif

/* Logging */

#define L_DEBUG		'D'		/* Debugging messages */
#define L_INFO		'I'		/* Informational msgs, warnings and errors */
#define L_WARN		'W'
#define L_ERROR		'E'
#define L_INFO_R	'i'		/* Errors caused by external events */
#define L_WARN_R	'w'
#define L_ERROR_R	'e'
#define L_FATAL		'!'		/* die() */

extern char *log_title;			/* NULL - print no title, default is log_progname */
extern char *log_filename;		/* Expanded name of the current log file */
extern volatile int log_switch_nest;	/* log_switch() nesting counter, increment to disable automatic switches */
extern int log_pid;			/* 0 if shouldn't be logged */
extern int log_precise_timings;		/* Include microsecond timestamps in log messages */
extern void (*log_die_hook)(void);
struct tm;
extern void (*log_switch_hook)(struct tm *tm);

void log_msg(unsigned int cat, const char *msg, ...) FORMAT_CHECK(printf,2,3);
#define log log_msg
void vlog_msg(unsigned int cat, const char *msg, va_list args);
void die(const char *, ...) NONRET FORMAT_CHECK(printf,1,2);
void log_init(byte *argv0);
void log_file(byte *name);
void log_fork(void);
int log_switch(void);

void assert_failed(char *assertion, char *file, int line) NONRET;
void assert_failed_noinfo(void) NONRET;

#ifdef DEBUG_ASSERTS
#define ASSERT(x) do { if (unlikely(!(x))) assert_failed(#x, __FILE__, __LINE__); } while(0)
#else
#define ASSERT(x) do { if (__builtin_constant_p(x) && !(x)) assert_failed_noinfo(); } while(0)
#endif

#define COMPILE_ASSERT(name,x) typedef char _COMPILE_ASSERT_##name[!!(x)-1]

#ifdef LOCAL_DEBUG
#define DBG(x,y...) log(L_DEBUG, x,##y)
#else
#define DBG(x,y...) do { } while(0)
#endif

static inline void log_switch_disable(void) { log_switch_nest++; }
static inline void log_switch_enable(void) { ASSERT(log_switch_nest); log_switch_nest--; }

/* Memory allocation */

#define xmalloc sh_xmalloc
#define xrealloc sh_xrealloc
#define xfree sh_xfree

#ifdef DEBUG_DMALLOC
/*
 * The standard dmalloc macros tend to produce lots of namespace
 * conflicts and we use only xmalloc and xfree, so we can define
 * the stubs ourselves.
 */
#define DMALLOC_DISABLE
#include <dmalloc.h>
#define sh_xmalloc(size) _xmalloc_leap(__FILE__, __LINE__, size)
#define sh_xrealloc(ptr,size) _xrealloc_leap(__FILE__, __LINE__, ptr, size)
#define sh_xfree(ptr) _xfree_leap(__FILE__, __LINE__, ptr)
#else
/*
 * Unfortunately, several libraries we might want to link to define
 * their own xmalloc and we don't want to interfere with them, hence
 * the renaming.
 */
void *xmalloc(unsigned) LIKE_MALLOC;
void *xrealloc(void *, unsigned);
void xfree(void *);
#endif

void *xmalloc_zero(unsigned) LIKE_MALLOC;
byte *xstrdup(byte *) LIKE_MALLOC;

/* Content-Type pattern matching and filters */

int match_ct_patt(byte *, byte *);

/* wordsplit.c */

int sepsplit(byte *str, byte sep, byte **rec, uns max);
int wordsplit(byte *, byte **, uns);

/* pat(i)match.c: Matching of shell patterns */

int match_pattern(byte *, byte *);
int match_pattern_nocase(byte *, byte *);

/* md5hex.c */

void md5_to_hex(byte *, byte *);
void hex_to_md5(byte *, byte *);

#define MD5_SIZE 16
#define MD5_HEX_SIZE 33

/* prime.c */

int isprime(uns);
uns nextprime(uns);

/* primetable.c */

uns next_table_prime(uns x);
uns prev_table_prime(uns x);

/* timer.c */

struct timeval;

void init_timer(void);
uns get_timer(void);
void get_last_timeval(struct timeval *tv);

/* regex.c */

typedef struct regex regex;

regex *rx_compile(byte *r, int icase);
void rx_free(regex *r);
int rx_match(regex *r, byte *s);
int rx_subst(regex *r, byte *by, byte *src, byte *dest, uns destlen);

/* random.c */

uns random_max(uns max);
u64 random_u64(void);
u64 random_max_u64(u64 max);

/* mmap.c */

void *mmap_file(byte *name, unsigned *len, int writeable);
void munmap_file(void *start, unsigned len);

/* proctitle.c */

void setproctitle_init(int argc, char **argv);
void setproctitle(char *msg, ...) FORMAT_CHECK(printf,1,2);
char *getproctitle(void);

/* randomkey.c */

void randomkey(byte *buf, uns size);

/* exitstatus.c */

#define EXIT_STATUS_MSG_SIZE 32
int format_exit_status(byte *msg, int stat);

/* runcmd.c */

int run_command(byte *cmd, ...);
void NONRET exec_command(byte *cmd, ...);
void echo_command(byte *buf, int size, byte *cmd, ...);
int run_command_v(byte *cmd, va_list args);
void NONRET exec_command_v(byte *cmd, va_list args);
void echo_command_v(byte *buf, int size, byte *cmd, va_list args);

/* carefulio.c */

int careful_read(int fd, void *buf, int len);
int careful_write(int fd, void *buf, int len);

/* sync.c */

void sync_dir(byte *name);

/* sighandler.c */

typedef int (*sh_sighandler_t)(int);	// gets signum, returns nonzero if abort() should be called

void handle_signal(int signum);
void unhandle_signal(int signum);
sh_sighandler_t set_signal_handler(int signum, sh_sighandler_t new);

/* string.c */

byte *str_unesc(byte *dest, byte *src);

/* bigalloc.c */

void *big_alloc(unsigned int len);
void big_free(void *start, unsigned int len);

#endif
