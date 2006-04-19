/*
 *      Knuth-Morris-Pratt's Substring Search for N given strings
 *
 *      (c) 1999--2005, Robert Spalek <robert@ucw.cz>
 *      (c) 2006, Pavel Charvat <pchar@ucw.cz>
 *
 *      (In fact, the algorithm is usually referred to as Aho-McCorasick,
 *      but that's just an extension of KMP to multiple strings.)
 */

/*
 *  This is not a normal header file, it's a generator of KMP algorithm.
 *  Each time you include it with parameters set in the corresponding
 *  preprocessor macros, it generates KMP structures and functions
 *  with the parameters given. Macros marked with [*] are mandatory.
 *
 *  [*]	KMPS_PREFIX(x)		macro to add a name prefix (used on all global names
 *				defined by the KMP search generator)
 *  [*]	KMPS_KMP_PREFIX(x)	prefix used for lib/kmp.h;
 *				more variants of kmp-search can be used for single lib/kmp.h
 *
 *  KMPS_SOURCE			user-defined search input (together with KMPS_GET_CHAR);
 *  				if unset, the one from lib/kmp.h is used
 *  KMPS_GET_CHAR(ctx,src,s)
 *
 *  KMPS_ADD_CONTROLS		add control characters at both ends of the input string
 *  KMPS_MERGE_CONTROLS 	merge adjacent control characters to a single one
 *
 *  KMPS_EXTRA_ARGS		extra arguments to the search routine
 *  KMPS_EXTRA_VAR		extra user-defined structure in search structures
 *  KMPS_INIT(ctx,src,s)
 *  KMPS_EXIT(ctx,src,s)
 *  KMPS_FOUND(ctx,src,s)
 *  KMPS_FOUND_CHAIN(ctx,src,s)
 *  KMPS_STEP(ctx,src,s)
 *  KMPS_T
 *
 *  KMPS_WANT_BEST
 */

#define P(x) KMPS_PREFIX(x)
#define KP(x) KMPS_KMP_PREFIX(x)

#ifdef KMPS_SOURCE
typedef KMPS_SOURCE P(search_source_t);
#else
typedef KP(source_t) P(search_source_t);
#endif

#ifndef KMPS_GET_CHAR
#define KMPS_GET_CHAR(ctx,src,s) ({ KP(get_char)(ctx, &src, &s.c); })
#endif

struct P(search) {
  struct KP(state) *s;		/* current state */
  struct KP(state) *out;	/* output state */
# ifdef KMPS_WANT_BEST
  struct KP(state) *best;	/* longest match */
# endif
  KP(char_t) c;			/* last character */
# ifdef KMPS_EXTRA_VAR
  KMPS_EXTRA_VAR v;		/* user-defined */
# endif
# ifdef KMPS_ADD_CONTROLS
  uns eof;
# endif
};

#ifdef KMPS_T
static KMPS_T
#else
static void
#endif
P(search) (struct KP(context) *ctx, P(search_source_t) src
#   ifdef KMPS_EXTRA_ARGS
    , KMPS_EXTRA_ARGS
#   endif
)
{
  struct P(search) s;
  s.s = &ctx->null;
# ifdef KMPS_WANT_BEST
  s.best = &ctx->null;
# endif
# ifdef KMPS_ADD_CONTROLS 
  s.c = KP(control_char)();
  s.eof = 0;
# else
  s.c = 0;
# endif  
# ifdef KMPS_INIT
  { KMPS_INIT(ctx, src, s); }
# endif
# ifndef KMPS_ADD_CONTROLS  
  goto start_read;
#endif  
  for (;;)
  {
    for (struct KP(state) *t = s.s; t && !(s.s = KP(hash_find)(&ctx->hash, t, s.c)); t = t->back);
    s.s = s.s ? : &ctx->null;

#   ifdef KMPS_STEP
    { KMPS_STEP(ctx, src, s); }
#   endif

#   if defined(KMPS_FOUND) || defined(KMPS_FOUND_CHAIN) || defined(KMPS_WANT_BEST)
    s.out = s.s->len ? s.s : s.s->next;
    if (s.out)
      {
#       ifdef KMPS_WANT_BEST
	if (s.out->len > s.best->len)
	  s.best = s.out;
#       endif	
        #ifdef KMPS_FOUND_CHAIN
	{ KMPS_FOUND_CHAIN(ctx, src, s); }
#       endif
#       ifdef KMPS_FOUND
	do
          { KMPS_FOUND(ctx, src, s); }
	while (s.out = s.out->next);
#       endif	
      }
#   endif

#   ifdef KMPS_ADD_CONTROLS    
    if (s.eof)
      break;
#   endif    

#   ifndef KMPS_ADD_CONTROLS    
start_read: ;
#   endif    
#   ifdef KMPS_MERGE_CONTROLS
    KP(char_t) last_c = s.c;
#   endif

    do
      {
	if (!KMPS_GET_CHAR(ctx, src, s))
	  {
#           ifdef KMPS_ADD_CONTROLS
	    if (s.c != KP(control_char)())
	      {
                s.c = KP(control_char)();
                s.eof = 1;
		break;
	      }
#           endif
	    goto exit;
	  }
      }
    while (0
#     ifdef KMPS_MERGE_CONTROLS
      || (last_c == KP(control_char)() && s.c == KP(control_char)())
#     endif
      );
  }
exit: ;
# ifdef KMPS_EXIT
  { KMPS_EXIT(ctx, src, s); }
# endif
}

#undef P
#undef KMPS_PREFIX
#undef KMPS_KMP_PREFIX
#undef KMPS_SOURCE
#undef KMPS_GET_CHAR
#undef KMPS_ADD_CONTROLS
#undef KMPS_MERGE_CONTROLS
#undef KMPS_EXTRA_ARGS
#undef KMPS_EXTRA_VAR
#undef KMPS_INIT
#undef KMPS_EXIT
#undef KMPS_FOUND
#undef KMPS_FOUND_CHAIN
#undef KMPS_STEP
#undef KMPS_T
#undef KMPS_WANT_BEST
