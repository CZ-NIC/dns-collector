/*
 *	Sherlock Library -- Poor Man's Profiler
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

/*
 *  Usage:
 *		prof_t cnt;
 *		prof_init(&cnt);
 *		...
 *		prof_start(&cnt);
 *		...
 *		prof_stop(&cnt);
 *		printf("%s\n", PROF_STRING(&cnt));
 */

/* Profiling method to use */
#define CONFIG_PROFILE_TOD		/* gettimeofday() */
#undef CONFIG_PROFILE_TSC		/* i386 TSC */
#undef CONFIG_PROFILE_KTSC		/* kernel TSC profiler */

#ifdef CONFIG_PROFILE_TOD
#define CONFIG_PROFILE
#define PROF_STR_SIZE 21

typedef struct {
  u32 start_sec, start_usec;
  s32 sec, usec;
} prof_t;

#endif

#ifdef CONFIG_PROFILE_TSC
#define CONFIG_PROFILE
#define CONFIG_PROFILE_INLINE
#define PROF_STR_SIZE 24

typedef struct {
  u64 start_tsc;
  u64 ticks;
} prof_t;

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

static inline void prof_start(prof_t *c)
{
  rdtscll(c->start_tsc);
}

static inline void prof_stop(prof_t *c)
{
  u64 tsc;
  rdtscll(tsc);
  tsc -= c->start_tsc;
  c->ticks += tsc;
}

static inline void prof_switch(prof_t *o, prof_t *n)
{
  u64 tsc;
  rdtscll(tsc);
  n->start_tsc = tsc;
  tsc -= o->start_tsc;
  o->ticks += tsc;
}
#endif

#ifdef CONFIG_PROFILE_KTSC
#define CONFIG_PROFILE
#define PROF_STR_SIZE 50

typedef struct {
  u64 start_user, start_sys;
  u64 ticks_user, ticks_sys;
} prof_t;
#endif

#ifdef CONFIG_PROFILE

/* Stuff common for all profilers */
#ifndef CONFIG_PROFILE_INLINE
void prof_switch(prof_t *, prof_t *);
static inline void prof_start(prof_t *c) { prof_switch(NULL, c); }
static inline void prof_stop(prof_t *c) { prof_switch(c, NULL); }
#endif
int prof_format(char *, prof_t *);
void prof_init(prof_t *);

#else

/* Dummy profiler with no output */
typedef struct { } prof_t;
static inline void prof_init(prof_t *c UNUSED) { }
static inline void prof_start(prof_t *c UNUSED) { }
static inline void prof_stop(prof_t *c UNUSED) { }
static inline void prof_switch(prof_t *c UNUSED, prof_t *d UNUSED) { }
static inline void prof_format(char *b, prof_t *c UNUSED) { strcpy(b, "?"); }
#define PROF_STR_SIZE 2

#endif
