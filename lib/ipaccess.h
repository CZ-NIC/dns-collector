/*
 *	Sherlock Library -- IP address access lists
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 */

struct ipaccess_list;

struct ipaccess_list *ipaccess_init(void);
byte *ipaccess_parse(struct ipaccess_list *l, byte *c, int is_allow);
int ipaccess_check(struct ipaccess_list *l, u32 ip);
