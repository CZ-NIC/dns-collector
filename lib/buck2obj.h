/*
 *	Bucket -> Object converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

int extract_odes(struct obuck_header *hdr, struct fastbuf *body, struct odes *o, byte *buf, uns buf_len, struct lizard_buffer *lizard_buf);
