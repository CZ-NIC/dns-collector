/*
 *	Bucket -> Object converter
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

struct buck2obj_buf;
struct mempool;

struct buck2obj_buf *buck2obj_alloc(void);
void buck2obj_free(struct buck2obj_buf *buf);
struct odes *obj_read_bucket(struct buck2obj_buf *buf, struct mempool *pool, uns buck_type, uns buck_len, struct fastbuf *body, uns *body_start);
  /* If body_start != NULL, then only the header is parsed and *body_start is
   * set to the position of the body. This function does a plenty of optimizations
   * and if the body fastbuf is overwritable (body->can_overwrite), it can keep the
   * attribute values stored on their original locations in the fastbuf's buffer.
   * However, no such things are performed when reading the header only.
   */
