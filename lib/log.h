/*
 *	Sherlock Library -- Logging
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#define L_DEBUG "<0>"
#define L_INFO "<2>"
#define L_WARN "<4>"
#define L_ERROR "<6>"
#define L_FATAL "<9>"

int log(byte *, ...);
void initlog(byte *);
