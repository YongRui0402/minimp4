/*
 * Edge case tests: NULL pointers, zero-length data, invalid parameters,
 * callback failures, and other boundary conditions.
 */
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#include "test_harness.h"

/* ─── Helpers ─────────────────────────────────────────────── */

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t size;
} mem_buffer_t;

static int write_cb(int64_t offset, const void *buffer, size_t size, void *token)
{
    mem_buffer_t *buf = (mem_buffer_t *)token;
    size_t needed = (size_t)offset + size;
    if (needed > buf->capacity) {
        size_t cap = buf->capacity ? buf->capacity : 4096;
        while (cap < needed) cap *= 2;
        uint8_t *tmp = (uint8_t *)realloc(buf->data, cap);
        if (!tmp) return 1;
        buf->data = tmp;
        buf->capacity = cap;
    }
    memcpy(buf->data + offset, buffer, size);
    if (needed > buf->size) buf->size = needed;
    return 0;
}

static int failing_write_cb(int64_t offset, const void *buffer, size_t size, void *token)
{
    (void)offset; (void)buffer; (void)size; (void)token;
    return 1; /* always fail */
}

static int read_cb(int64_t offset, void *buffer, size_t size, void *token)
{
    mem_buffer_t *buf = (mem_buffer_t *)token;
    if ((size_t)offset + size > buf->size) return 1;
    memcpy(buffer, buf->data + offset, size);
    return 0;
}

static int failing_read_cb(int64_t offset, void *buffer, size_t size, void *token)
{
    (void)offset; (void)buffer; (void)size; (void)token;
    return 1; /* always fail */
}

/* ─── MP4E_close edge cases ───────────────────────────────── */

TEST(test_close_null)
{
    int rc = MP4E_close(NULL);
    ASSERT_EQ(rc, MP4E_STATUS_BAD_ARGUMENTS);
}

/* ─── MP4E_open with no tracks, immediate close ───────────── */

TEST(test_open_close_no_tracks)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    ASSERT_NOT_NULL(mux);
    int rc = MP4E_close(mux);
    ASSERT_EQ(rc, MP4E_STATUS_OK);
    free(buf.data);
}

/* ─── set_dsi twice ───────────────────────────────────────── */

TEST(test_set_dsi_twice)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    MP4E_track_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.track_media_kind = e_audio;
    tr.object_type_indication = MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3;
    tr.time_scale = 44100;
    tr.default_duration = 1024;
    tr.u.a.channelcount = 2;
    tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';
    int tid = MP4E_add_track(mux, &tr);

    uint8_t dsi[] = {0x12, 0x10};
    ASSERT_EQ(MP4E_set_dsi(mux, tid, dsi, sizeof(dsi)), MP4E_STATUS_OK);
    /* Second call should fail */
    ASSERT_EQ(MP4E_set_dsi(mux, tid, dsi, sizeof(dsi)), MP4E_STATUS_ONLY_ONE_DSI_ALLOWED);

    MP4E_close(mux);
    free(buf.data);
}

/* ─── Demux with invalid data ─────────────────────────────── */

TEST(test_demux_truncated_data)
{
    /* A very short buffer that isn't valid MP4 */
    uint8_t bad_data[] = { 0x00, 0x00, 0x00, 0x08, 'f', 'r', 'e', 'e' };
    mem_buffer_t rbuf = { bad_data, 0, sizeof(bad_data) };
    MP4D_demux_t mp4;
    memset(&mp4, 0, sizeof(mp4));

    int rc = MP4D_open(&mp4, read_cb, &rbuf, sizeof(bad_data));
    /* Should either fail or return 0 tracks */
    if (rc == 1) {
        ASSERT_EQ(mp4.track_count, 0);
    }
    MP4D_close(&mp4);
}

TEST(test_demux_zero_size)
{
    uint8_t dummy = 0;
    mem_buffer_t rbuf = { &dummy, 0, 0 };
    MP4D_demux_t mp4;
    memset(&mp4, 0, sizeof(mp4));

    int rc = MP4D_open(&mp4, read_cb, &rbuf, 0);
    ASSERT_EQ(rc, 0);
    MP4D_close(&mp4);
}

TEST(test_demux_failing_read_callback)
{
    MP4D_demux_t mp4;
    memset(&mp4, 0, sizeof(mp4));

    int rc = MP4D_open(&mp4, failing_read_cb, NULL, 1000);
    /* Should fail since it can't read */
    ASSERT_EQ(rc, 0);
    MP4D_close(&mp4);
}

/* ─── read_sps/read_pps out-of-range ──────────────────────── */

TEST(test_read_sps_pps_out_of_range)
{
    /* Create a minimal valid MP4 */
    mem_buffer_t mp4buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &mp4buf, write_cb);
    MP4E_track_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.track_media_kind = e_video;
    tr.object_type_indication = MP4_OBJECT_TYPE_AVC;
    tr.time_scale = 90000;
    tr.default_duration = 3000;
    tr.u.v.width = 320; tr.u.v.height = 240;
    tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';
    int tid = MP4E_add_track(mux, &tr);
    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a, 0xf8, 0x41, 0xa2};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    MP4E_set_sps(mux, tid, sps, sizeof(sps));
    MP4E_set_pps(mux, tid, pps, sizeof(pps));
    uint8_t frame[32];
    memset(frame, 0xAB, sizeof(frame));
    MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, MP4E_SAMPLE_RANDOM_ACCESS);
    MP4E_close(mux);

    /* Demux */
    mem_buffer_t rbuf = { mp4buf.data, mp4buf.capacity, mp4buf.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4buf.size);

    /* Out-of-range SPS index */
    int nbytes = 0;
    ASSERT_NULL(MP4D_read_sps(&demux, 0, 99, &nbytes));
    ASSERT_NULL(MP4D_read_pps(&demux, 0, 99, &nbytes));

    /* Out-of-range track index — should return NULL (or crash without bounds check) */
    /* We test with a valid but non-existent second track index */
    if (demux.track_count == 1) {
        /* track index 1 is out of range but we can't safely test this
           without bounds checking in the library */
    }

    MP4D_close(&demux);
    free(mp4buf.data);
}

/* ─── Multiple tracks, close properly ─────────────────────── */

TEST(test_close_multi_track)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);

    /* Add 5 video tracks */
    for (int i = 0; i < 5; i++) {
        MP4E_track_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.track_media_kind = e_video;
        tr.object_type_indication = MP4_OBJECT_TYPE_AVC;
        tr.time_scale = 90000;
        tr.default_duration = 3000;
        tr.u.v.width = 320; tr.u.v.height = 240;
        tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';
        int tid = MP4E_add_track(mux, &tr);
        ASSERT_EQ(tid, i);

        uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a};
        uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
        MP4E_set_sps(mux, tid, sps, sizeof(sps));
        MP4E_set_pps(mux, tid, pps, sizeof(pps));
    }

    /* Close should free all tracks without leaks (ASan will catch) */
    MP4E_close(mux);
    free(buf.data);
}

/* ─── Empty text comment ──────────────────────────────────── */

TEST(test_empty_text_comment)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    int rc = MP4E_set_text_comment(mux, "");
    ASSERT_EQ(rc, MP4E_STATUS_OK);
    MP4E_close(mux);
    free(buf.data);
}

/* ─── Large sample count ──────────────────────────────────── */

TEST(test_many_samples)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);

    MP4E_track_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.track_media_kind = e_video;
    tr.object_type_indication = MP4_OBJECT_TYPE_AVC;
    tr.time_scale = 90000;
    tr.default_duration = 3000;
    tr.u.v.width = 160; tr.u.v.height = 120;
    tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';
    int tid = MP4E_add_track(mux, &tr);

    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    MP4E_set_sps(mux, tid, sps, sizeof(sps));
    MP4E_set_pps(mux, tid, pps, sizeof(pps));

    uint8_t frame[16];
    memset(frame, 0x42, sizeof(frame));

    /* Write 500 samples */
    for (int i = 0; i < 500; i++) {
        int kind = (i % 30 == 0) ? MP4E_SAMPLE_RANDOM_ACCESS : MP4E_SAMPLE_DEFAULT;
        ASSERT_EQ(MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, kind), MP4E_STATUS_OK);
    }

    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);

    /* Verify we can demux it back */
    mem_buffer_t rbuf = { buf.data, buf.capacity, buf.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    int rc = MP4D_open(&demux, read_cb, &rbuf, (int64_t)buf.size);
    ASSERT_EQ(rc, 1);
    ASSERT_EQ(demux.track[0].sample_count, 500);

    MP4D_close(&demux);
    free(buf.data);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_edge_cases\n");
    RUN_TEST(test_close_null);
    RUN_TEST(test_open_close_no_tracks);
    RUN_TEST(test_set_dsi_twice);
    RUN_TEST(test_demux_truncated_data);
    RUN_TEST(test_demux_zero_size);
    RUN_TEST(test_demux_failing_read_callback);
    RUN_TEST(test_read_sps_pps_out_of_range);
    RUN_TEST(test_close_multi_track);
    RUN_TEST(test_empty_text_comment);
    RUN_TEST(test_many_samples);
    TEST_SUMMARY();
}
