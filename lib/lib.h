/*
 *	The UCW Library -- Miscellaneous Functions
 *
 *	(c) 1997--2008 Martin Mares <mj@ucw.cz>
 *	(c) 2005 Tomas Valla <tom@ucw.cz>
 *	(c) 2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_LIB_H
#define _UCW_LIB_H

#include "lib/config.h"
#include <stdarg.h>

/* Macros for handling structurues, offsets and alignment */

#define CHECK_PTR_TYPE(x, type) ((x)-(type)(x) + (type)(x))
#define PTR_TO(s, i) &((s*)0)->i
#define OFFSETOF(s, i) ((unsigned int) PTR_TO(s, i))
#define SKIP_BACK(s, i, p) ((s *)((char *)p - OFFSETOF(s, i)))
#define ALIGN_TO(s, a) (((s)+a-1)&~(a-1))
#define ALIGN_PTR(p, s) ((uintptr_t)(p) % (s) ? (typeof(p))((uintptr_t)(p) + (s) - (uintptr_t)(p) % (s)) : (p))
#define UNALIGNED_PART(ptr, type) (((uintptr_t) (ptr)) % sizeof(type))

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

#define L_SIGHANDLER	0x10000		/* Avoid operations that are unsafe in signal handlers */

extern char *log_title;			/* NULL - print no title, default is program name given to log_init() */
extern char *log_filename;		/* Expanded name of the current log file */
extern int log_pid;			/* 0 if shouldn't be logged */
extern int log_precise_timings;		/* Include microsecond timestamps in log messages */
extern void (*log_die_hook)(void);
struct tm;
extern void (*log_switch_hook)(struct tm *tm);

void msg(uns cat, const char *fmt, ...) FORMAT_CHECK(printf,2,3);
void vmsg(uns cat, const char *fmt, va_list args);
void die(const char *, ...) NONRET FORMAT_CHECK(printf,1,2);
void log_init(const char *argv0);
void log_file(const char *name);
void log_fork(void);			/* Call after fork() to update log_pid */

/* If the log name contains metacharacters for date and time, we switch the logs
 * automatically whenever the name changes. You can disable it and switch explicitly. */
int log_switch(void);
void log_switch_disable(void);
void log_switch_enable(void);

void assert_failed(const char *assertion, const char *file, int line) NONRET;
void assert_failed_noinfo(void) NONRET;

#ifdef DEBUG_ASSERTS
#define ASSERT(x) ({ if (unlikely(!(x))) assert_failed(#x, __FILE__, __LINE__); 1; })
#else
#define ASSERT(x) ({ if (__builtin_constant_p(x) && !(x)) assert_failed_noinfo(); 1; })
#endif

#define COMPILE_ASSERT(name,x) typedef char _COMPILE_ASSERT_##name[!!(x)-1]

#ifdef LOCAL_DEBUG
#define DBG(x,y...) msg(L_DEBUG, x,##y)
#else
#define DBG(x,y...) do { } while(0)
#endif

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
void *xmalloc(uns) LIKE_MALLOC;
void *xrealloc(void *, uns);
void xfree(void *);
#endif

void *xmalloc_zero(uns) LIKE_MALLOC;
char *xstrdup(const char *) LIKE_MALLOC;

/* Content-Type pattern matching and filters */

int match_ct_patt(const char *, const char *);

/* wordsplit.c */

int sepsplit(char *str, uns sep, char **rec, uns max);
int wordsplit(char *str, char **rec, uns max);

/* pat(i)match.c: Matching of shell patterns */

int match_pattern(const char *patt, const char *str);
int match_pattern_nocase(const char *patt, const char *str);

/* md5hex.c */

void md5_to_hex(const byte *s, char *d);
void hex_to_md5(const char *s, byte *d);

#define MD5_SIZE 16
#define MD5_HEX_SIZE 33

/* prime.c */

int isprime(uns x);
uns nextprime(uns x);

/* primetable.c */

uns next_table_prime(uns x);
uns prev_table_prime(uns x);

/* timer.c */

timestamp_t get_timestamp(void);

void init_timer(timestamp_t *timer);
uns get_timer(timestamp_t *timer);
uns switch_timer(timestamp_t *old, timestamp_t *new);

/* regex.c */

typedef struct regex regex;

regex *rx_compile(const char *r, int icase);
void rx_free(regex *r);
int rx_match(regex *r, const char *s);
int rx_subst(regex *r, const char *by, const char *src, char *dest, uns destlen);

/* random.c */

uns random_u32(void);
uns random_max(uns max);
u64 random_u64(void);
u64 random_max_u64(u64 max);

/* mmap.c */

void *mmap_file(const char *name, unsigned *len, int writeable);
void munmap_file(void *start, unsigned len);

/* proctitle.c */

void setproctitle_init(int argc, char **argv);
void setproctitle(const char *msg, ...) FORMAT_CHECK(printf,1,2);
char *getproctitle(void);

/* randomkey.c */

void randomkey(byte *buf, uns size);

/* exitstatus.c */

#define EXIT_STATUS_MSG_SIZE 32
int format_exit_status(char *msg, int stat);

/* runcmd.c */

int run_command(const char *cmd, ...);
void NONRET exec_command(const char *cmd, ...);
void echo_command(char *buf, int size, const char *cmd, ...);
int run_command_v(const char *cmd, va_list args);
void NONRET exec_command_v(const char *cmd, va_list args);
void echo_command_v(char *buf, int size, const char *cmd, va_list args);

/* carefulio.c */

int careful_read(int fd, void *buf, int len);
int careful_write(int fd, const void *buf, int len);

/* sync.c */

void sync_dir(const char *name);

/* sighandler.c */

typedef int (*sh_sighandler_t)(int);	// gets signum, returns nonzero if abort() should be called

void handle_signal(int signum);
void unhandle_signal(int signum);
sh_sighandler_t set_signal_handler(int signum, sh_sighandler_t new);

/* string.c */

char *str_unesc(char *dest, const char *src);
char *str_format_flags(char *dest, const char *fmt, uns flags);

/* bigalloc.c */

void *page_alloc(u64 len) LIKE_MALLOC; // allocates a multiple of CPU_PAGE_SIZE bytes with mmap
void *page_alloc_zero(u64 len) LIKE_MALLOC;
void page_free(void *start, u64 len);
void *page_realloc(void *start, u64 old_len, u64 new_len);

void *big_alloc(u64 len) LIKE_MALLOC; // allocate a large memory block in the most efficient way available
void *big_alloc_zero(u64 len) LIKE_MALLOC;
void big_free(void *start, u64 len);

#endif
