/*
 *	Sherlock Library -- IP address access lists
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lists.h"

typedef list ipaccess_list;

void ipaccess_init(ipaccess_list *l);
byte *ipaccess_parse(ipaccess_list *l, byte *c, int is_allow);
int ipaccess_check(ipaccess_list *l, u32 ip);
