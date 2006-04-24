/*
 *	UCW Library -- Strings Allocated on the Stack
 *
 *	(c) 2005--2006 Martin Mares <mj@ucw.cz>
 *	(c) 2005 Tomas Valla <tom@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <alloca.h>
#include <string.h>

#define stk_strdup(s) ({ char *_s=(s); uns _l=strlen(_s)+1; char *_x=alloca(_l); memcpy(_x, _s, _l); _x; })
#define stk_strndup(s,n) ({ char *_s=(s); uns _l=strnlen(_s,(n)); char *_x=alloca(_l+1); memcpy(_x, _s, _l); _x[_l]=0; _x; })
#define stk_strcat(s1,s2) ({ char *_s1=(s1); char *_s2=(s2); uns _l1=strlen(_s1); uns _l2=strlen(_s2); char *_x=alloca(_l1+_l2+1); memcpy(_x,_s1,_l1); memcpy(_x+_l1,_s2,_l2+1); _x; })
#define stk_strmulticat(s...) ({ char *_s[]={s}; char *_x=alloca(stk_array_len(_s, ARRAY_SIZE(_s)-1)); stk_array_copy(_x, _s, ARRAY_SIZE(_s)-1); _x; })
#define stk_strarraycat(s,n) ({ char **_s=(s); int _n=(n); char *_x=alloca(stk_array_len(_s,_n)); stk_array_copy(_x, _s, _n); _x; })
#define stk_printf(f...) ({ uns _l=stk_printf_internal(f); char *_x=alloca(_l); memcpy(_x, stk_printf_buf, _l); _x; })
#define stk_hexdump(s,n) ({ uns _n=(n); char *_x=alloca(3*_n+1); stk_hexdump_internal(_x,(byte*)(s),_n); _x; })
#define stk_str_unesc(s) ({ byte *_s=(s); byte *_d=alloca(strlen(_s)+1); str_unesc(_d, _s); _d; })

uns stk_array_len(char **s, uns cnt);
void stk_array_copy(char *x, char **s, uns cnt);
uns stk_printf_internal(char *x, ...) FORMAT_CHECK(printf,1,2);
void stk_hexdump_internal(char *dst, byte *src, uns n);

extern char *stk_printf_buf;
