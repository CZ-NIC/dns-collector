/*
 *	Sherlock Library -- Character Types
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_CHARTYPE_H
#define _SHERLOCK_CHARTYPE_H

#define _C_UPPER 1			/* Upper-case letters */
#define _C_LOWER 2			/* Lower-case letters */
#define _C_PRINT 4			/* Printable */
#define _C_DIGIT 8			/* Digits */
#define _C_CTRL 16			/* Control characters */
#define _C_XDIGIT 32			/* Hexadecimal digits */
#define _C_BLANK 64			/* Blanks */
#define _C_INNER 128			/* `inner punctuation' -- underscore etc. */

#define _C_ALPHA (_C_UPPER | _C_LOWER)
#define _C_ALNUM (_C_ALPHA | _C_DIGIT)
#define _C_WORD (_C_ALNUM | _C_INNER)
#define _C_WSTART (_C_ALPHA | _C_INNER)

extern unsigned char _c_cat[256], _c_upper[256], _c_collate[256], _c_order[256];

#define Category(x) (_c_cat[(unsigned char)(x)])
#define Ccat(x,y) (Category(x) & y)

#define Cupper(x) Ccat(x, _C_UPPER)
#define Clower(x) Ccat(x, _C_LOWER)
#define Calpha(x) Ccat(x, _C_ALPHA)
#define Calnum(x) Ccat(x, _C_ALNUM)
#define Cprint(x) Ccat(x, _C_PRINT)
#define Cdigit(x) Ccat(x, _C_DIGIT)
#define Cxdigit(x) Ccat(x, _C_XDIGIT)
#define Cword(x) Ccat(x, _C_WORD)
#define Cblank(x) Ccat(x, _C_BLANK)
#define Cctrl(x) Ccat(x, _C_CTRL)
#define Cspace(x) Cblank(x)

#define Cupcase(x) _c_upper[(unsigned char)(x)]

#define Cxvalue(x) (((x)<'A')?((x)-'0'):(((x)&0xdf)-'A'+10))

#endif
