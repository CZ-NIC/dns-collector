/*
 *	Bucket -> Object converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

struct buck2obj_buf;
struct mempool;

struct buck2obj_buf *buck2obj_alloc(struct mempool *mp);
void buck2obj_free(struct buck2obj_buf *buf);
void buck2obj_flush(struct buck2obj_buf *buf);
struct odes *obj_read_bucket(struct buck2obj_buf *buf, uns buck_type, uns buck_len, struct fastbuf *body, uns *body_start);
  /* If body_start != NULL, then only the header is parsed and *body_start is
   * set to the position of the body.
   *
   * If BCONFIG_CAN_OVERWRITE(body)==2, then the buffer of body has probably
   * been tampered with (unless the bucket is larger than the buffer).  In such
   * a case, you must call bflush(body) before you do anything else than
   * sequential read.  */

/* FIXME: These functions should be put somewhere else */
byte *obj_attr_to_bucket(byte *buf, uns buck_type, uns attr, byte *val);
byte *obj_attr_to_bucket_num(byte *buf, uns buck_type, uns attr, uns val);
