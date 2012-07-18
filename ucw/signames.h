/*
 *	A List of Signal Names
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 */

#ifndef _UCW_SIGNAMES_H
#define _UCW_SIGNAMES_H

int sig_name_to_number(const char *name);

const char *sig_number_to_name(int number);

#endif
