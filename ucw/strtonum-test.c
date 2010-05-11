/*
 *	UCW Library -- Conversions of Strings to Numbers: Testing
 *
 *      (c) 2010 Daniel Fiala <danfiala@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/chartype.h"
#include "ucw/strtonum.h"

#include <stdio.h>

static uns str_to_flags(const char *str)
{
   uns flags = 0;
   for(const char *p = str; *p; ++p)
     {
       switch(*p)
         {
           case 'h':
             flags = (flags & ~STN_DBASES_MASK) | 16;
             break;
           case '8':
             flags = (flags & ~STN_DBASES_MASK) | 8;
             break;
           case '2':
             flags = (flags & ~STN_DBASES_MASK) | 2;
             break;
           case '0':
             flags = (flags & ~STN_DBASES_MASK) | 10;
             break;

           case 'X':
             flags |= STN_HEX;
             break;
           case 'o':
             flags |= STN_OCT;
             break;
           case 'B':
             flags |= STN_BIN;
             break;
           case 'D':
             flags |= STN_DEC;
             break;

           case '_':
             flags |= STN_UNDERSCORE;
             break;
           case 't':
             flags |= STN_TRUNC;
             break;
           case '+':
             flags |= STN_PLUS;
             break;
           case '-':
             flags |= STN_MINUS;
             break;
           case 's':
             flags |= STN_SIGNED;
             break;
           case 'Z':
             flags |= STN_ZCHAR;
             break;
         }
     }

  return flags;
}

static void convert(const char *str_flags, const char *str_num)
{
  const uns flags = str_to_flags(str_flags);
 
  const char *next1, *next2;
  uns ux = 1234567890;
  uintmax_t um = 1234567890;
  const char *err1 = str_to_uns(&ux, str_num, &next1, flags);
  const char *err2 = str_to_uintmax(&um, str_num, &next2, flags);

  if (flags & STN_SIGNED)
    printf("i%d\nh%x\ne[%s]\nc[%s]\nb%td:0x%x\nI%jd\nH%jx\nE[%s]\nC[%s]\nB%td:0x%x\n", ux, ux, err1, str_num, next1 - str_num, *next1, um, um, err2, str_num, next2 - str_num, *next2);
  else
    printf("i%u\nh%x\ne[%s]\nc[%s]\nb%td:0x%x\nI%ju\nH%jx\nE[%s]\nC[%s]\nB%td:0x%x\n", ux, ux, err1, str_num, next1 - str_num, *next1, um, um, err2, str_num, next2 - str_num, *next2);
}

int main(int argc, char *argv[])
{
  if (argc >= 3)
    convert(argv[1], argv[2]);

  return 0;
}
