/*
 *  Regular Expression Functions from glibc 2.3.2
 *  (renamed to sh_* to avoid clashes with the system libraries)
 */

#ifndef _SHERLOCK_REGEX_H
#define _SHERLOCK_REGEX_H

#define regfree sh_regfree
#define regexec sh_regexec
#define regcomp sh_regcomp
#define regerror sh_regerror
#define re_set_registers sh_re_set_registers
#define re_match_2 sh_re_match2
#define re_match sh_re_match
#define re_search sh_re_search
#define re_compile_pattern sh_re_compile_pattern
#define re_set_syntax sh_re_set_syntax
#define re_search_2 sh_re_search_2
#define re_compile_fastmap sh_re_compile_fastmap

#include "lib/regex/regex.h"

#endif
