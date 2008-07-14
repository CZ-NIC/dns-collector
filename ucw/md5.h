/*
 *	UCW Library -- MD5 Message Digest
 *
 *	This file is in public domain (see ucw/md5.c).
 */

#ifndef _UCW_MD5_H
#define _UCW_MD5_H

typedef struct {
	u32 buf[4];
	u32 bits[2];
	byte in[64];
} md5_context;

void md5_init(md5_context *context);
void md5_update(md5_context *context, const byte *buf, uns len);
/* The data are stored inside the context */
byte *md5_final(md5_context *context);

void md5_transform(u32 buf[4], const u32 in[16]);

/* One-shot interface */
void md5_hash_buffer(byte *outbuf, const byte *buffer, uns length);

#define MD5_HEX_SIZE 33
#define MD5_SIZE 16

#endif /* !_UCW_MD5_H */
