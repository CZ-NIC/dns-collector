/*
 *	Sherlock Library -- URL Functions
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 */

#ifndef _SHERLOCK_URL_H
#define _SHERLOCK_URL_H

#define MAX_URL_SIZE 1024

/* Non-control meanings of control characters */

#define NCC_SEMICOLON 1
#define NCC_SLASH 2
#define NCC_QUEST 3
#define NCC_COLON 4
#define NCC_AT 5
#define NCC_EQUAL 6
#define NCC_AND 7
#define NCC_HASH 8
#define NCC_MAX 9

#define NCC_CHARS " ;/?:@=&#"

/* Remove/Introduce '%' escapes */

int url_deescape(byte *, byte *);
int url_enescape(byte *, byte *);

/* URL splitting and normalization */

struct url {
  byte *protocol;
  uns protoid;
  byte *user;
  byte *host;
  uns port;				/* ~0 if unspec */
  byte *rest;
  byte *buf, *bufend;
};

int url_split(byte *, struct url *, byte *);
int url_normalize(struct url *, struct url *);
int url_canonicalize(struct url *);
int url_pack(struct url *, byte *);
int url_canon_split(byte *, byte *, byte *, struct url *);
uns identify_protocol(byte *);
int url_has_repeated_component(byte *url);

/* Error codes */

char *url_error(uns);

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

extern byte *url_proto_names[];

#endif
