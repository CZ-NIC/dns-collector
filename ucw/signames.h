/*
 *	A List of Signal Names
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 */

#ifndef _UCW_SIGNAMES_H
#define _UCW_SIGNAMES_H

/***
 * POSIX lacks facilities for conversion between signal names
 * and signal numbers. They are available in LibUCW, but please
 * be aware that some signals might be missing on your system.
 * If they do, please notify LibUCW maintainers.
 ***/

/**
 * Converts signal name to the corresponding number.
 * Returns -1 if not found.
 **/
int sig_name_to_number(const char *name);

/**
 * Converts signal number to the corresponding name.
 * If more names are known for the given signal, one of them
 * is considered canonical and preferred.
 * Returns NULL if not found.
 **/
const char *sig_number_to_name(int number);

#endif
