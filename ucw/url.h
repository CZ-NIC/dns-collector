/*
 *	UCW Library -- URL Functions
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_URL_H
#define _UCW_URL_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define url_auto_canonicalize_rel ucw_url_auto_canonicalize_rel
#define url_canon_split_rel ucw_url_canon_split_rel
#define url_canonicalize ucw_url_canonicalize
#define url_deescape ucw_url_deescape
#define url_enescape ucw_url_enescape
#define url_enescape_friendly ucw_url_enescape_friendly
#define url_error ucw_url_error
#define url_has_repeated_component ucw_url_has_repeated_component
#define url_identify_protocol ucw_url_identify_protocol
#define url_normalize ucw_url_normalize
#define url_pack ucw_url_pack
#define url_proto_names ucw_url_proto_names
#define url_split ucw_url_split
#endif

#define MAX_URL_SIZE 1024

/* Non-control meanings of control characters */

enum {
  NCC_SEMICOLON = 1,
  NCC_SLASH = 2,
  NCC_QUEST = 3,
  NCC_COLON = 4,
  NCC_AT = 5,
  NCC_EQUAL = 6,
  NCC_AND = 7,
  NCC_HASH = 8,
  // Avoid 9 (\t) and 10 (\n)
  NCC_DOLLAR = 11,
  NCC_PLUS = 12,
  // Avoid 13 (\r)
  NCC_COMMA = 14,
  NCC_MAX = 15
};

#define NCC_CHARS " ;/?:@=&#\t\n$+\r,"

/* Remove/Introduce '%' escapes */

int url_deescape(const char *s, char *d);
int url_enescape(const char *s, char *d);
int url_enescape_friendly(const char *src, char *dest);

/* URL splitting and normalization */

struct url {
  char *protocol;
  uint protoid;
  char *user;
  char *pass;
  char *host;
  uint port;				/* ~0 if unspec */
  char *rest;
  char *buf, *bufend;
};

int url_split(char *s, struct url *u, char *d);
int url_normalize(struct url *u, struct url *b);
int url_canonicalize(struct url *u);
int url_pack(struct url *u, char *d);
int url_canon_split_rel(const char *url, char *buf1, char *buf2, struct url *u, struct url *base);
int url_auto_canonicalize_rel(const char *src, char *dst, struct url *base);
uint url_identify_protocol(const char *p);
int url_has_repeated_component(const char *url);

static inline int url_canon_split(const char *url, char *buf1, char *buf2, struct url *u)
{ return url_canon_split_rel(url, buf1, buf2, u, NULL); }

static inline int url_auto_canonicalize(const char *src, char *dst)
{ return url_auto_canonicalize_rel(src, dst, NULL); }

/* Error codes */

char *url_error(uint);

#define URL_ERR_TOO_LONG 1
#define URL_ERR_INVALID_CHAR 2
#define URL_ERR_INVALID_ESCAPE 3
#define URL_ERR_INVALID_ESCAPED_CHAR 4
#define URL_ERR_INVALID_PORT 5
#define URL_ERR_REL_NOTHING 6
#define URL_ERR_UNKNOWN_PROTOCOL 7
#define URL_SYNTAX_ERROR 8
#define URL_PATH_UNDERFLOW 9

#define URL_PROTO_UNKNOWN 0
#define URL_PROTO_HTTP 1
#define URL_PROTO_FTP 2
#define URL_PROTO_FILE 3
#define URL_PROTO_MAX 4

#define URL_PNAMES { "unknown", "http", "ftp", "file" }
#define URL_DEFPORTS { ~0, 80, 21, 0 }
#define URL_PATH_FLAGS { 0, 1, 1, 1 }

extern char *url_proto_names[];

#endif
