/*
 * libFuzzer target for MP4D_open (demuxer).
 * Feeds arbitrary bytes as an MP4 file to the demuxer.
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_demux fuzz_demux.c
 */
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

typedef struct {
    const uint8_t *data;
    size_t size;
} fuzz_buffer_t;

static int fuzz_read_cb(int64_t offset, void *buffer, size_t size, void *token)
{
    fuzz_buffer_t *buf = (fuzz_buffer_t *)token;
    if (offset < 0 || (size_t)offset + size > buf->size)
        return 1;
    memcpy(buffer, buf->data + offset, size);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 8 || size > 10 * 1024 * 1024)
        return 0;

    fuzz_buffer_t buf = { data, size };
    MP4D_demux_t mp4;
    memset(&mp4, 0, sizeof(mp4));

    if (MP4D_open(&mp4, fuzz_read_cb, &buf, (int64_t)size)) {
        /* Try to read some frames if tracks exist */
        for (unsigned t = 0; t < mp4.track_count && t < 4; t++) {
            unsigned limit = mp4.track[t].sample_count;
            if (limit > 16) limit = 16;
            for (unsigned s = 0; s < limit; s++) {
                unsigned frame_bytes, timestamp, duration;
                MP4D_frame_offset(&mp4, t, s, &frame_bytes, &timestamp, &duration);
            }

            /* Try reading SPS/PPS */
            int nbytes;
            for (int i = 0; i < 4; i++) {
                MP4D_read_sps(&mp4, t, i, &nbytes);
                MP4D_read_pps(&mp4, t, i, &nbytes);
            }
        }
        MP4D_close(&mp4);
    }

    return 0;
}
