/*
 *	Generating V33 buckets
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

void attr_set_type(uns type);

byte *put_attr(byte *ptr, uns type, byte *val, uns len);
byte *put_attr_str(byte *ptr, uns type, byte *val);
byte *put_attr_vformat(byte *ptr, uns type, byte *mask, va_list va);
byte *put_attr_format(byte *ptr, uns type, char *mask, ...) __attribute__((format(printf,3,4)));
byte *put_attr_num(byte *ptr, uns type, uns val);

struct fastbuf;
void bput_attr(struct fastbuf *b, uns type, byte *val, uns len);
void bput_attr_str(struct fastbuf *b, uns type, byte *val);
void bput_attr_vformat(struct fastbuf *b, uns type, byte *mask, va_list va);
void bput_attr_format(struct fastbuf *b, uns type, char *mask, ...) __attribute__((format(printf,3,4)));
void bput_attr_num(struct fastbuf *b, uns type, uns val);
