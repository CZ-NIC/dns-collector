/*
 *	Bucket -> Object converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

struct buck2obj_buf;

struct buck2obj_buf *buck2obj_alloc(uns max_len, struct mempool *mp);
void buck2obj_free(struct buck2obj_buf *buf);
struct odes *buck2obj_convert(struct buck2obj_buf *buf, struct obuck_header *hdr, struct fastbuf *body);
