#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#include "test_harness.h"

/* In-memory write target */
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
        if (!tmp)
            return 1;
        buf->data = tmp;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + offset, buffer, size);
    if (needed > buf->size)
        buf->size = needed;
    return 0;
}

static int failing_write_cb(int64_t offset, const void *buffer, size_t size, void *token)
{
    (void)offset; (void)buffer; (void)size; (void)token;
    return 1;
}

static void mem_buffer_init(mem_buffer_t *buf)
{
    memset(buf, 0, sizeof(*buf));
}

static void mem_buffer_free(mem_buffer_t *buf)
{
    free(buf->data);
    memset(buf, 0, sizeof(*buf));
}

/* ─── Tests ───────────────────────────────────────────────── */

TEST(test_open_default_mode)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);

    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    ASSERT_NOT_NULL(mux);
    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    mem_buffer_free(&buf);
}

TEST(test_open_sequential_mode)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);

    MP4E_mux_t *mux = MP4E_open(1, 0, &buf, write_cb);
    ASSERT_NOT_NULL(mux);
    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    mem_buffer_free(&buf);
}

TEST(test_open_fragmentation_mode)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);

    MP4E_mux_t *mux = MP4E_open(0, 1, &buf, write_cb);
    ASSERT_NOT_NULL(mux);
    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    mem_buffer_free(&buf);
}

TEST(test_add_video_track)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    ASSERT_NOT_NULL(mux);

    MP4E_track_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.track_media_kind = e_video;
    tr.object_type_indication = MP4_OBJECT_TYPE_AVC;
    tr.time_scale = 90000;
    tr.default_duration = 90000 / 30;
    tr.u.v.width = 352;
    tr.u.v.height = 288;
    tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';

    int track_id = MP4E_add_track(mux, &tr);
    ASSERT_GE(track_id, 0);

    MP4E_close(mux);
    mem_buffer_free(&buf);
}

TEST(test_add_audio_track)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    ASSERT_NOT_NULL(mux);

    MP4E_track_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.track_media_kind = e_audio;
    tr.object_type_indication = MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3;
    tr.time_scale = 44100;
    tr.default_duration = 1024;
    tr.u.a.channelcount = 2;
    tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';

    int track_id = MP4E_add_track(mux, &tr);
    ASSERT_GE(track_id, 0);

    MP4E_close(mux);
    mem_buffer_free(&buf);
}

TEST(test_add_multiple_tracks)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);

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
    atr.u.a.channelcount = 1;
    atr.language[0] = 'u'; atr.language[1] = 'n'; atr.language[2] = 'd';

    int vid = MP4E_add_track(mux, &vtr);
    int aud = MP4E_add_track(mux, &atr);
    ASSERT_GE(vid, 0);
    ASSERT_GE(aud, 0);
    ASSERT_NE(vid, aud);

    MP4E_close(mux);
    mem_buffer_free(&buf);
}

TEST(test_put_sample_video)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);

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

    /* Fake SPS/PPS */
    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a, 0xf8, 0x41, 0xa2};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    ASSERT_EQ(MP4E_set_sps(mux, tid, sps, sizeof(sps)), MP4E_STATUS_OK);
    ASSERT_EQ(MP4E_set_pps(mux, tid, pps, sizeof(pps)), MP4E_STATUS_OK);

    /* Put some fake samples */
    uint8_t frame[64];
    memset(frame, 0xAB, sizeof(frame));

    int rc = MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, MP4E_SAMPLE_RANDOM_ACCESS);
    ASSERT_EQ(rc, MP4E_STATUS_OK);

    rc = MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, MP4E_SAMPLE_DEFAULT);
    ASSERT_EQ(rc, MP4E_STATUS_OK);

    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    mem_buffer_free(&buf);
}

TEST(test_set_dsi)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
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

    uint8_t dsi[] = {0x12, 0x10};  /* AAC-LC 44100Hz stereo */
    int rc = MP4E_set_dsi(mux, tid, dsi, sizeof(dsi));
    ASSERT_EQ(rc, MP4E_STATUS_OK);

    /* Second set_dsi should fail */
    rc = MP4E_set_dsi(mux, tid, dsi, sizeof(dsi));
    ASSERT_EQ(rc, MP4E_STATUS_ONLY_ONE_DSI_ALLOWED);

    MP4E_close(mux);
    mem_buffer_free(&buf);
}

TEST(test_set_text_comment)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);

    int rc = MP4E_set_text_comment(mux, "test comment");
    ASSERT_EQ(rc, MP4E_STATUS_OK);

    MP4E_close(mux);
    mem_buffer_free(&buf);
}

TEST(test_put_sample_continuation)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);

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

    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    MP4E_set_sps(mux, tid, sps, sizeof(sps));
    MP4E_set_pps(mux, tid, pps, sizeof(pps));

    uint8_t data[32];
    memset(data, 0xCC, sizeof(data));

    /* Key frame */
    ASSERT_EQ(MP4E_put_sample(mux, tid, data, sizeof(data), 3000, MP4E_SAMPLE_RANDOM_ACCESS), MP4E_STATUS_OK);
    /* Continuation (slice) */
    ASSERT_EQ(MP4E_put_sample(mux, tid, data, sizeof(data), 0, MP4E_SAMPLE_CONTINUATION), MP4E_STATUS_OK);
    /* Normal frame */
    ASSERT_EQ(MP4E_put_sample(mux, tid, data, sizeof(data), 3000, MP4E_SAMPLE_DEFAULT), MP4E_STATUS_OK);

    MP4E_close(mux);
    mem_buffer_free(&buf);
}

TEST(test_sequential_with_samples)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(1, 0, &buf, write_cb);

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

    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    MP4E_set_sps(mux, tid, sps, sizeof(sps));
    MP4E_set_pps(mux, tid, pps, sizeof(pps));

    uint8_t frame[64];
    memset(frame, 0xDD, sizeof(frame));

    for (int i = 0; i < 10; i++) {
        int kind = (i == 0) ? MP4E_SAMPLE_RANDOM_ACCESS : MP4E_SAMPLE_DEFAULT;
        ASSERT_EQ(MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, kind), MP4E_STATUS_OK);
    }

    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    mem_buffer_free(&buf);
}

TEST(test_fragmented_with_samples)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 1, &buf, write_cb);

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

    uint8_t sps[] = {0x67, 0x42, 0x00, 0x0a};
    uint8_t pps[] = {0x68, 0xce, 0x38, 0x80};
    MP4E_set_sps(mux, tid, sps, sizeof(sps));
    MP4E_set_pps(mux, tid, pps, sizeof(pps));

    uint8_t frame[64];
    memset(frame, 0xEE, sizeof(frame));

    for (int i = 0; i < 10; i++) {
        int kind = (i % 5 == 0) ? MP4E_SAMPLE_RANDOM_ACCESS : MP4E_SAMPLE_DEFAULT;
        ASSERT_EQ(MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, kind), MP4E_STATUS_OK);
    }

    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    mem_buffer_free(&buf);
}

TEST(test_hevc_track)
{
    mem_buffer_t buf;
    mem_buffer_init(&buf);
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);

    MP4E_track_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.track_media_kind = e_video;
    tr.object_type_indication = MP4_OBJECT_TYPE_HEVC;
    tr.time_scale = 90000;
    tr.default_duration = 3000;
    tr.u.v.width = 352;
    tr.u.v.height = 288;
    tr.language[0] = 'u'; tr.language[1] = 'n'; tr.language[2] = 'd';

    int tid = MP4E_add_track(mux, &tr);
    ASSERT_GE(tid, 0);

    /* Fake VPS/SPS/PPS for HEVC */
    uint8_t vps[] = {0x40, 0x01, 0x0c, 0x01};
    uint8_t sps[] = {0x42, 0x01, 0x01, 0x01};
    uint8_t pps[] = {0x44, 0x01, 0xc1, 0x73};
    ASSERT_EQ(MP4E_set_vps(mux, tid, vps, sizeof(vps)), MP4E_STATUS_OK);
    ASSERT_EQ(MP4E_set_sps(mux, tid, sps, sizeof(sps)), MP4E_STATUS_OK);
    ASSERT_EQ(MP4E_set_pps(mux, tid, pps, sizeof(pps)), MP4E_STATUS_OK);

    uint8_t frame[32];
    memset(frame, 0xFF, sizeof(frame));
    ASSERT_EQ(MP4E_put_sample(mux, tid, frame, sizeof(frame), 3000, MP4E_SAMPLE_RANDOM_ACCESS), MP4E_STATUS_OK);

    MP4E_close(mux);
    mem_buffer_free(&buf);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_mux_api\n");
    RUN_TEST(test_open_default_mode);
    RUN_TEST(test_open_sequential_mode);
    RUN_TEST(test_open_fragmentation_mode);
    RUN_TEST(test_add_video_track);
    RUN_TEST(test_add_audio_track);
    RUN_TEST(test_add_multiple_tracks);
    RUN_TEST(test_put_sample_video);
    RUN_TEST(test_set_dsi);
    RUN_TEST(test_set_text_comment);
    RUN_TEST(test_put_sample_continuation);
    RUN_TEST(test_sequential_with_samples);
    RUN_TEST(test_fragmented_with_samples);
    RUN_TEST(test_hevc_track);
    TEST_SUMMARY();
}
