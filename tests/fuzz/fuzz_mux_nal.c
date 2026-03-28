/*
 * libFuzzer target for mp4_h26x_write_nal.
 * Feeds arbitrary bytes as NAL units to the muxer.
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_mux_nal fuzz_mux_nal.c
 */
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

static int null_write_cb(int64_t offset, const void *buffer, size_t size, void *token)
{
    (void)offset; (void)buffer; (void)size; (void)token;
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 5 || size > 1024 * 1024)
        return 0;

    MP4E_mux_t *mux = MP4E_open(0, 0, NULL, null_write_cb);
    if (!mux)
        return 0;

    mp4_h26x_writer_t wr;
    if (MP4E_STATUS_OK != mp4_h26x_write_init(&wr, mux, 320, 240, 0)) {
        MP4E_close(mux);
        return 0;
    }

    /* Feed the fuzzer data as a single NAL unit */
    mp4_h26x_write_nal(&wr, data, (int)size, 3000);

    mp4_h26x_write_close(&wr);
    MP4E_close(mux);
    return 0;
}
