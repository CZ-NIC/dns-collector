#include <lz4.h>
#include <lz4frame.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "output.h"
#include "output_compression.h"

const char *dns_output_compression_names[] = {
      "none", "lz4-fast", "lz4-med", "lz4-best", NULL
};


static LZ4F_preferences_t lz4fast_preferences = {
    { LZ4F_max64KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0, { 0, 0 } },
    0, 0,
    { 0, 0, 0, 0 },  /* reserved */
};


static LZ4F_preferences_t lz4med_preferences = {
    { LZ4F_max256KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0, { 0, 0 } },
    4, 0,
    { 0, 0, 0, 0 },  /* reserved */
};


static LZ4F_preferences_t lz4best_preferences = {
    { LZ4F_max1MB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0, { 0, 0 } },
    9, 0,
    { 0, 0, 0, 0 },  /* reserved */
};


static void
dns_output_write_helper(const char *buf, size_t len, FILE *f)
{
    size_t n, pos = 0;
    while (pos < len) {
        n = fwrite(buf + pos, 1, len - pos, f);
        if ((n < len - pos) && ferror(f))
            msg(L_ERROR | DNS_MSG_SPAM, "Output write error %d", ferror(f));
        pos += n;
    }
}


void
dns_output_start_compression(struct dns_output *out)
{
    if (out->compression == dns_oc_lz4fast || out->compression == dns_oc_lz4med || out->compression == dns_oc_lz4best) {
        size_t n;

        n = LZ4F_createCompressionContext(&(out->lz4_ctx), LZ4F_VERSION);
        if (LZ4F_isError(n))
            die("LZ4 error: %s", LZ4F_getErrorName(n));

        switch (out->compression) {
            case dns_oc_lz4fast: out->lz4_prefs = &lz4fast_preferences; break;
            case dns_oc_lz4med: out->lz4_prefs = &lz4med_preferences; break;
            case dns_oc_lz4best: out->lz4_prefs = &lz4best_preferences; break;
            default: assert(0);
        }

        out->lz4_buf_len = LZ4F_compressBound(DNS_OUTPUT_LZ4_WRITE_SIZE, out->lz4_prefs) + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
        out->lz4_buf = (char *)xmalloc(out->lz4_buf_len);

        n = LZ4F_compressBegin(out->lz4_ctx, out->lz4_buf, out->lz4_buf_len, out->lz4_prefs);
        if (LZ4F_isError(n))
            die("LZ4 error: %s", LZ4F_getErrorName(n));
        dns_output_write_helper(out->lz4_buf, n, out->f);
        out->wrote_bytes_compressed += n;
    }
}


void
dns_output_finish_compression(struct dns_output *out)
{
    if (out->compression == dns_oc_lz4fast || out->compression == dns_oc_lz4med || out->compression == dns_oc_lz4best) {
        size_t n;

        n = LZ4F_compressEnd(out->lz4_ctx, out->lz4_buf, out->lz4_buf_len, NULL);
        if (LZ4F_isError(n))
            die("LZ4 error: %s", LZ4F_getErrorName(n));
        dns_output_write_helper(out->lz4_buf, n, out->f);
        out->wrote_bytes_compressed += n;

        LZ4F_freeCompressionContext(out->lz4_ctx);
        xfree(out->lz4_buf);
        out->lz4_buf_len = 0;
    }
}


void
dns_output_write(struct dns_output *out, const char *buf, size_t len)
{
    assert(out && buf);

    if (len == 0)
        return;

    out->wrote_bytes += len;

    if (out->compression == dns_oc_none) {
        dns_output_write_helper(buf, len, out->f);
        out->wrote_bytes_compressed += len;
        return;
    }

    if (out->compression == dns_oc_lz4fast || out->compression == dns_oc_lz4med || out->compression == dns_oc_lz4best) {
        while(len > 0) {
            size_t n, wr = MIN(len, DNS_OUTPUT_LZ4_WRITE_SIZE);
            n = LZ4F_compressUpdate(out->lz4_ctx, out->lz4_buf, out->lz4_buf_len, buf, wr, NULL);
            dns_output_write_helper(out->lz4_buf, n, out->f);
            out->wrote_bytes_compressed += n;
            buf += wr;
            len -= wr;
        }
        return;
    }
    die("unreachable");
}


