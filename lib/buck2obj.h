/*
 *	Bucket -> Object converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

struct buck2obj_buf;

struct buck2obj_buf *buck2obj_alloc(struct mempool *mp);
void buck2obj_free(struct buck2obj_buf *buf);
struct odes *obj_read_bucket(struct buck2obj_buf *buf, uns buck_type, struct fastbuf *body, uns want_body);
  /* If BCONFIG_CAN_OVERWRITE(body)==2, then the buffer of body has probably
   * been tampered with (unless the bucket is larger than the buffer).  In such
   * a case, you must call bflush(body) before you do anything else than
   * sequential read.  */
