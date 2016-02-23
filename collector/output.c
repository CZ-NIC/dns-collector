#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>

#include <lz4.h>
#include <lz4frame.h>
#define LZ4_HEADER_SIZE 20
#define LZ4_FOOTER_SIZE 8

#include "common.h"
#include "output.h"


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


char *dns_output_init(struct dns_output *out)
{
    out->period_sec = 300.0; // 5 min

    out->compression = dns_oc_none;

    return NULL;
}


char *dns_output_commit(struct dns_output *out)
{
    if (out->period_sec < 0.000001)
        out->period = 0;
    else
        out->period = dns_fsec_to_us_time(out->period_sec);

    if (!out->path_template)
        return "'path_template' needed in output.";

    if (out->compression >= dns_oc_LAST)
        return "Invalid compression.";

    return NULL;
}

#define DNS_OUTPUT_LZ4_WRITE_SIZE (1024 * 4)

static void
dns_output_writebuf_helper(const char *buf, size_t len, FILE *f)
{
    size_t n, pos = 0;
    while (pos < len) {
        n = fwrite(buf + pos, 1, len - pos, f);
        if ((n < len - pos) && ferror(f))
            msg(L_ERROR, "Output write error %d", ferror(f));
        pos += n;
    }
}

void
dns_output_open(struct dns_output *out, dns_us_time_t time)
{
    assert(out && !out->f && out->manage_files && (time != DNS_NO_TIME));

    char path[PATH_MAX];
    size_t l = dns_us_time_strftime(path, PATH_MAX, out->path_template, time);

    if (l == 0)
        die("Expanded filename '%s' too long.", out->path_template);
        
    out->f = fopen(path, "w");
    if (!out->f)
        die("Unable to open output file '%s': %s.", path, strerror(errno));
    out->f_time_opened = time;

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
        dns_output_writebuf_helper(out->lz4_buf, n, out->f);
    }

    if (out->start_file)
          out->start_file(out, time);
}


void
dns_output_close(struct dns_output *out, dns_us_time_t time)
{
    assert(out && out->f && out->manage_files && time != DNS_NO_TIME);

    if (out->finish_file)
          out->finish_file(out, time);

    if (out->compression == dns_oc_lz4fast || out->compression == dns_oc_lz4med || out->compression == dns_oc_lz4best) {
        size_t n;

        n = LZ4F_compressEnd(out->lz4_ctx, out->lz4_buf, out->lz4_buf_len, NULL);
        if (LZ4F_isError(n))
            die("LZ4 error: %s", LZ4F_getErrorName(n));
        dns_output_writebuf_helper(out->lz4_buf, n, out->f);

        LZ4F_freeCompressionContext(out->lz4_ctx);
        xfree(out->lz4_buf);
        out->lz4_buf_len = 0;
    }

    fclose(out->f);
    out->f = NULL;
}


void
dns_output_check_rotation(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (time != DNS_NO_TIME) && out->manage_files);

    // check if we need to switch output files
    if ((out->period > 0) && (out->f) && (time >= out->f_time_opened + out->period))
        dns_output_close(out, time);

    // open if not open
    if (!out->f)
        dns_output_open(out, time);
}
// TODO: Sync wile rotation with respect to drop/write

void
dns_output_write(struct dns_output *out, const char *buf, size_t len)
{
    assert(out && buf);

    if (out->compression == dns_oc_none) {
        dns_output_writebuf_helper(buf, len, out->f);
        return;
    }

    if (out->compression == dns_oc_lz4fast || out->compression == dns_oc_lz4med || out->compression == dns_oc_lz4best) {
        while(len > 0) {
            size_t n, wr = MIN(len, DNS_OUTPUT_LZ4_WRITE_SIZE);
            n = LZ4F_compressUpdate(out->lz4_ctx, out->lz4_buf, out->lz4_buf_len, buf, wr, NULL);
            dns_output_writebuf_helper(out->lz4_buf, n, out->f);
            buf += wr;
            len -= wr;
        }
        return;
    }
    die("unreachable");
}


