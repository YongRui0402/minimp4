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
        size_t new_cap = buf->capacity ? buf->capacity : 4096;
        while (new_cap < needed)
            new_cap *= 2;
        uint8_t *tmp = (uint8_t *)realloc(buf->data, new_cap);
        if (!tmp) return 1;
        buf->data = tmp;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + offset, buffer, size);
    if (needed > buf->size)
        buf->size = needed;
    return 0;
}

typedef struct {
    const uint8_t *data;
    size_t size;
} read_buffer_t;

static int read_cb(int64_t offset, void *buffer, size_t size, void *token)
{
    read_buffer_t *buf = (read_buffer_t *)token;
    if ((size_t)offset + size > buf->size)
        return 1;
    memcpy(buffer, buf->data + offset, size);
    return 0;
}

/* Create a minimal valid MP4 in memory with N video frames, return the buffer.
 * Caller must free mp4_out->data. */
static void create_test_mp4(mem_buffer_t *mp4_out, int n_frames, int sequential, int fragmented)
{
    memset(mp4_out, 0, sizeof(*mp4_out));

    MP4E_mux_t *mux = MP4E_open(sequential, fragmented, mp4_out, write_cb);

    MP4E_track_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.track_media_kind = e_video;
    tr.object_type_indication = MP4_OBJECT_TYPE_AVC;
    tr.time_scale = 90000;
    tr.default_duration = 3000;
    tr.u.v.width = 320;
    tr.u.v.height = 240;
    tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';
    int tid = MP4E_add_track(mux, &tr);

    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a, 0xf8, 0x41, 0xa2};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    MP4E_set_sps(mux, tid, sps, sizeof(sps));
    MP4E_set_pps(mux, tid, pps, sizeof(pps));

    for (int i = 0; i < n_frames; i++) {
        uint8_t frame[48];
        memset(frame, (uint8_t)(i & 0xFF), sizeof(frame));
        int kind = (i == 0) ? MP4E_SAMPLE_RANDOM_ACCESS : MP4E_SAMPLE_DEFAULT;
        MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, kind);
    }

    MP4E_close(mux);
}

/* ─── Tests ───────────────────────────────────────────────── */

TEST(test_demux_open_valid)
{
    mem_buffer_t mp4;
    create_test_mp4(&mp4, 5, 0, 0);
    ASSERT_GT(mp4.size, 0);

    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));

    int rc = MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);
    ASSERT_EQ(rc, 1);
    ASSERT_GE(demux.track_count, 1);

    MP4D_close(&demux);
    free(mp4.data);
}

TEST(test_demux_track_info)
{
    mem_buffer_t mp4;
    create_test_mp4(&mp4, 10, 0, 0);

    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);

    ASSERT_EQ(demux.track_count, 1);

    MP4D_track_t *tr = &demux.track[0];
    ASSERT_EQ(tr->handler_type, MP4D_HANDLER_TYPE_VIDE);
    ASSERT_EQ(tr->sample_count, 10);
    ASSERT_EQ(tr->object_type_indication, MP4_OBJECT_TYPE_AVC);

    MP4D_close(&demux);
    free(mp4.data);
}

TEST(test_demux_frame_offset)
{
    mem_buffer_t mp4;
    create_test_mp4(&mp4, 5, 0, 0);

    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);

    for (unsigned i = 0; i < demux.track[0].sample_count; i++) {
        unsigned frame_bytes = 0, timestamp = 0, duration = 0;
        MP4D_file_offset_t ofs = MP4D_frame_offset(&demux, 0, i, &frame_bytes, &timestamp, &duration);
        ASSERT_GT(frame_bytes, 0);
        ASSERT_GT(ofs, 0);
        ASSERT_GT(duration, 0);
    }

    MP4D_close(&demux);
    free(mp4.data);
}

TEST(test_demux_read_sps_pps)
{
    mem_buffer_t mp4;
    create_test_mp4(&mp4, 3, 0, 0);

    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);

    /* Should have at least one SPS and one PPS */
    int sps_bytes = 0;
    const void *sps = MP4D_read_sps(&demux, 0, 0, &sps_bytes);
    ASSERT_NOT_NULL(sps);
    ASSERT_GT(sps_bytes, 0);

    int pps_bytes = 0;
    const void *pps = MP4D_read_pps(&demux, 0, 0, &pps_bytes);
    ASSERT_NOT_NULL(pps);
    ASSERT_GT(pps_bytes, 0);

    /* Second SPS/PPS should be NULL (we only set one each) */
    ASSERT_NULL(MP4D_read_sps(&demux, 0, 1, &sps_bytes));
    ASSERT_NULL(MP4D_read_pps(&demux, 0, 1, &pps_bytes));

    MP4D_close(&demux);
    free(mp4.data);
}

TEST(test_demux_sequential_mp4)
{
    mem_buffer_t mp4;
    create_test_mp4(&mp4, 5, 1, 0);

    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));

    int rc = MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);
    ASSERT_EQ(rc, 1);
    ASSERT_EQ(demux.track[0].sample_count, 5);

    MP4D_close(&demux);
    free(mp4.data);
}

TEST(test_demux_frame_data_integrity)
{
    mem_buffer_t mp4;
    create_test_mp4(&mp4, 3, 0, 0);

    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);

    /* Verify each frame's data can be read from the offset */
    for (unsigned i = 0; i < demux.track[0].sample_count; i++) {
        unsigned frame_bytes = 0, timestamp = 0, duration = 0;
        MP4D_file_offset_t ofs = MP4D_frame_offset(&demux, 0, i, &frame_bytes, &timestamp, &duration);

        /* The offset + frame_bytes should not exceed the MP4 size */
        ASSERT_TRUE((size_t)ofs + frame_bytes <= mp4.size);

        /* Verify we can actually read the data at that offset */
        ASSERT_NOT_NULL(mp4.data + ofs);
    }

    MP4D_close(&demux);
    free(mp4.data);
}

TEST(test_demux_close_idempotent)
{
    mem_buffer_t mp4;
    create_test_mp4(&mp4, 2, 0, 0);

    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);

    MP4D_close(&demux);
    /* After close, track should be NULL */
    ASSERT_NULL(demux.track);

    free(mp4.data);
}

TEST(test_demux_multitrack)
{
    /* Create MP4 with video + audio tracks */
    mem_buffer_t mp4;
    memset(&mp4, 0, sizeof(mp4));

    MP4E_mux_t *mux = MP4E_open(0, 0, &mp4, write_cb);

    MP4E_track_t vtr;
    memset(&vtr, 0, sizeof(vtr));
    vtr.track_media_kind = e_video;
    vtr.object_type_indication = MP4_OBJECT_TYPE_AVC;
    vtr.time_scale = 90000;
    vtr.default_duration = 3000;
    vtr.u.v.width = 320;
    vtr.u.v.height = 240;
    vtr.language[0] = 'u'; vtr.language[1] = 'n'; vtr.language[2] = 'd';

    MP4E_track_t atr;
    memset(&atr, 0, sizeof(atr));
    atr.track_media_kind = e_audio;
    atr.object_type_indication = MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3;
    atr.time_scale = 44100;
    atr.default_duration = 1024;
    atr.u.a.channelcount = 2;
    atr.language[0] = 'u'; atr.language[1] = 'n'; atr.language[2] = 'd';

    int vid = MP4E_add_track(mux, &vtr);
    int aud = MP4E_add_track(mux, &atr);

    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    MP4E_set_sps(mux, vid, sps, sizeof(sps));
    MP4E_set_pps(mux, vid, pps, sizeof(pps));

    uint8_t dsi[] = {0x12, 0x10};
    MP4E_set_dsi(mux, aud, dsi, sizeof(dsi));

    uint8_t vframe[32], aframe[16];
    memset(vframe, 0xAA, sizeof(vframe));
    memset(aframe, 0xBB, sizeof(aframe));

    for (int i = 0; i < 5; i++) {
        int vkind = (i == 0) ? MP4E_SAMPLE_RANDOM_ACCESS : MP4E_SAMPLE_DEFAULT;
        MP4E_put_sample(mux, vid, vframe, sizeof(vframe), 3000, vkind);
        MP4E_put_sample(mux, aud, aframe, sizeof(aframe), 1024, MP4E_SAMPLE_RANDOM_ACCESS);
    }

    MP4E_close(mux);

    /* Now demux and verify two tracks */
    read_buffer_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    int rc = MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size);
    ASSERT_EQ(rc, 1);
    ASSERT_EQ(demux.track_count, 2);

    /* Find video and audio tracks */
    int found_video = 0, found_audio = 0;
    for (unsigned t = 0; t < demux.track_count; t++) {
        if (demux.track[t].handler_type == MP4D_HANDLER_TYPE_VIDE) {
            found_video = 1;
            ASSERT_EQ(demux.track[t].sample_count, 5);
        } else if (demux.track[t].handler_type == MP4D_HANDLER_TYPE_SOUN) {
            found_audio = 1;
            ASSERT_EQ(demux.track[t].sample_count, 5);
        }
    }
    ASSERT_TRUE(found_video);
    ASSERT_TRUE(found_audio);

    MP4D_close(&demux);
    free(mp4.data);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_demux_api\n");
    RUN_TEST(test_demux_open_valid);
    RUN_TEST(test_demux_track_info);
    RUN_TEST(test_demux_frame_offset);
    RUN_TEST(test_demux_read_sps_pps);
    RUN_TEST(test_demux_sequential_mp4);
    RUN_TEST(test_demux_frame_data_integrity);
    RUN_TEST(test_demux_close_idempotent);
    RUN_TEST(test_demux_multitrack);
    TEST_SUMMARY();
}
