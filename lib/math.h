/*
 *	Sherlock Library -- Stub for including math.h, avoiding name collisions
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef log
#define log libm_log
#define exception math_exception
#include <math.h>
#undef log
#define log log_msg
#undef exception
