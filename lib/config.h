/*
 *	Sherlock Library -- Configuration-Dependent Definitions
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

/* Version */

#define SHER_VER "0.1"

/* Types */

typedef unsigned char byte;		/* exactly 8 bits, unsigned */
typedef signed char sbyte;		/* exactly 8 bits, signed */
typedef unsigned short word;		/* exactly 16 bits, unsigned */
typedef short sword;			/* exactly 16 bits, signed */
typedef unsigned int ulg;		/* exactly 32 bits, unsigned */
typedef int slg;			/* exactly 32 bits, signed */
typedef unsigned int uns;		/* at least 32 bits */

/* Misc */

#ifdef __GNUC__

#undef inline
#define NONRET __attribute__((noreturn))

#else

#define inline
#define NONRET

#endif

#ifdef linux
#define HAVE_FFS
#endif
