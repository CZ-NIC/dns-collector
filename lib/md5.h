/*
 *	UCW Library -- MD5 Message Digest
 *
 *	This file is in public domain (see lib/md5.c).
 */

#ifndef _UCW_MD5_H
#define _UCW_MD5_H

typedef u32 uint32;

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(uint32 buf[4], uint32 const in[16]);

#define MD5_HEX_SIZE 33
#define MD5_SIZE 16

#endif /* !_UCW_MD5_H */
