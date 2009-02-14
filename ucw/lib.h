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

#include "ucw/config.h"
#include <stdarg.h>

/* Macros for handling structurues, offsets and alignment */

#define CHECK_PTR_TYPE(x, type) ((x)-(type)(x) + (type)(x))
#define PTR_TO(s, i) &((s*)0)->i
#define OFFSETOF(s, i) ((unsigned int) (uintptr_t) PTR_TO(s, i))
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

#define	ROL(x, bits) (((x) << (bits)) | ((uns)(x) >> (sizeof(uns)*8 - (bits))))	/* Bitwise rotation of an uns to the left */
#define	ROR(x, bits) (((uns)(x) >> (bits)) | ((x) << (sizeof(uns)*8 - (bits))))

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

/***
 * [[logging]]
 *
 * === Basic logging functions (see <<log:,Logging>> and <ucw/log.h> for more)
 ***/

enum log_levels {			/** The available log levels to pass to msg() and friends. **/
  L_DEBUG=0,				// 'D' - Debugging
  L_INFO,				// 'I' - Informational
  L_WARN,				// 'W' - Warning
  L_ERROR,				// 'E' - Error, but non-critical
  L_INFO_R,				// 'i' - An alternative set of levels for messages caused by remote events
  L_WARN_R,				// 'w'   (e.g., a packet received via network)
  L_ERROR_R,				// 'e'
  L_FATAL,				// '!' - Fatal error
};

#define L_SIGHANDLER 0x80000000		/** Avoid operations that are unsafe in signal handlers **/

/**
 * This is the basic printf-like function for logging a message.
 * The @flags contain the log level and possibly other flag bits (like @L_SIGHANDLER).
 **/
void msg(uns flags, const char *fmt, ...) FORMAT_CHECK(printf,2,3);
void vmsg(uns flags, const char *fmt, va_list args);		/** A vararg version of msg(). **/
void die(const char *, ...) NONRET FORMAT_CHECK(printf,1,2);	/** Log a fatal error message and exit the program. **/

extern char *log_title;			/** An optional log message title. Set to program name by log_init(). **/
extern int log_pid;			/** An optional PID printed in each log message. Set to 0 if it shouldn't be logged. **/
extern void (*log_die_hook)(void);	/** An optional function called just before die() exists. **/

void log_init(const char *argv0);	/** Set @log_title to the program name extracted from @argv[0]. **/
void log_fork(void);			/** Call after fork() to update @log_pid. **/
void log_file(const char *name);	/** Establish logging to the named file. Also redirect stderr there. **/

void assert_failed(const char *assertion, const char *file, int line) NONRET;
void assert_failed_noinfo(void) NONRET;

#ifdef DEBUG_ASSERTS
/**
 * Check an assertion. If the condition @x is false, stop the program with a fatal error.
 * These checks are compiled only when @DEBUG_ASSERTS is defined.
 **/
#define ASSERT(x) ({ if (unlikely(!(x))) assert_failed(#x, __FILE__, __LINE__); 1; })
#else
#define ASSERT(x) ({ if (__builtin_constant_p(x) && !(x)) assert_failed_noinfo(); 1; })
#endif

#define COMPILE_ASSERT(name,x) typedef char _COMPILE_ASSERT_##name[!!(x)-1]

#ifdef LOCAL_DEBUG
#define DBG(x,y...) msg(L_DEBUG, x,##y)	/** If @LOCAL_DEBUG is defined before including <ucw/lib.h>, log a debug message. Otherwise do nothing. **/
#else
#define DBG(x,y...) do { } while(0)
#endif

/* Memory allocation */

#define xmalloc ucw_xmalloc
#define xrealloc ucw_xrealloc
#define xfree ucw_xfree

/*
 * Unfortunately, several libraries we might want to link to define
 * their own xmalloc and we don't want to interfere with them, hence
 * the renaming.
 */
void *xmalloc(uns) LIKE_MALLOC;
void *xrealloc(void *, uns);
void xfree(void *);

void *xmalloc_zero(uns) LIKE_MALLOC;
char *xstrdup(const char *) LIKE_MALLOC;

/* timer.c */

timestamp_t get_timestamp(void);

void init_timer(timestamp_t *timer);
uns get_timer(timestamp_t *timer);
uns switch_timer(timestamp_t *oldt, timestamp_t *newt);

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

typedef int (*ucw_sighandler_t)(int);	// gets signum, returns nonzero if abort() should be called

void handle_signal(int signum);
void unhandle_signal(int signum);
ucw_sighandler_t set_signal_handler(int signum, ucw_sighandler_t newh);

/* bigalloc.c */

void *page_alloc(u64 len) LIKE_MALLOC; // allocates a multiple of CPU_PAGE_SIZE bytes with mmap
void *page_alloc_zero(u64 len) LIKE_MALLOC;
void page_free(void *start, u64 len);
void *page_realloc(void *start, u64 old_len, u64 new_len);

void *big_alloc(u64 len) LIKE_MALLOC; // allocate a large memory block in the most efficient way available
void *big_alloc_zero(u64 len) LIKE_MALLOC;
void big_free(void *start, u64 len);

#endif
