/*
 * Tests for the mp4_h26x_write_* convenience API.
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

/*
 * Realistic H.264 SPS/PPS/IDR NAL data.
 * The SPS ID patcher (MINIMP4_TRANSCODE_SPS_ID) parses the SPS header,
 * so we need valid H.264 SPS bitstream, not random bytes.
 *
 * SPS for Baseline profile, level 1.0, 176x144:
 *   nal_type=7, profile_idc=66, constraint_set_flags=0xC0, level_idc=10
 *   seq_parameter_set_id=0 (ue=0), log2_max_frame_num=0 (ue=0)
 *   pic_order_cnt_type=2 (ue=2), max_num_ref_frames=1 (ue=1)
 *   gaps_in_frame_num_allowed=0, pic_width_in_mbs_minus1=10 (ue=10)
 *   pic_height_in_map_units_minus1=8 (ue=8), frame_mbs_only=1
 */
/*
 * Use heap-allocated buffers built at runtime to avoid ASan global-buffer-overflow.
 * mp4_h26x_write_nal uses find_start_code internally which can scan past the end
 * of tight const arrays, so we allocate with padding.
 */
static uint8_t *make_nal_buf(const uint8_t *src, int src_len, int *out_len)
{
    int padded = src_len + 16; /* extra space for scanner overread */
    uint8_t *buf = (uint8_t *)calloc(1, padded);
    memcpy(buf, src, src_len);
    *out_len = src_len;
    return buf;
}

/* Raw NAL payloads (without padding) */
static const uint8_t raw_sps[] = {
    0x00, 0x00, 0x00, 0x01,
    0x67, 0x42, 0xC0, 0x0A, 0xDA, 0x0F, 0x0A, 0x69, 0xA8, 0x08, 0x08, 0x08
};
static const uint8_t raw_pps[] = {
    0x00, 0x00, 0x00, 0x01,
    0x68, 0xCE, 0x38, 0x80
};
static const uint8_t raw_idr[] = {
    0x00, 0x00, 0x00, 0x01,
    0x65, 0x88, 0x80, 0x40, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};
static const uint8_t raw_slice[] = {
    0x00, 0x00, 0x00, 0x01,
    0x41, 0x9A, 0x01, 0x40, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};

/* ─── Tests ───────────────────────────────────────────────── */

TEST(test_h26x_write_init_avc)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    ASSERT_NOT_NULL(mux);

    mp4_h26x_writer_t wr;
    int rc = mp4_h26x_write_init(&wr, mux, 320, 240, 0);
    ASSERT_EQ(rc, MP4E_STATUS_OK);
    ASSERT_EQ(wr.is_hevc, 0);
    ASSERT_EQ(wr.need_sps, 1);
    ASSERT_EQ(wr.need_pps, 1);
    ASSERT_EQ(wr.need_idr, 1);

    mp4_h26x_write_close(&wr);
    MP4E_close(mux);
    free(buf.data);
}

TEST(test_h26x_write_init_hevc)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    ASSERT_NOT_NULL(mux);

    mp4_h26x_writer_t wr;
    int rc = mp4_h26x_write_init(&wr, mux, 352, 288, 1);
    ASSERT_EQ(rc, MP4E_STATUS_OK);
    ASSERT_EQ(wr.is_hevc, 1);
    ASSERT_EQ(wr.need_vps, 1);
    ASSERT_EQ(wr.need_sps, 1);
    ASSERT_EQ(wr.need_pps, 1);
    ASSERT_EQ(wr.need_idr, 1);

    mp4_h26x_write_close(&wr);
    MP4E_close(mux);
    free(buf.data);
}

TEST(test_h26x_write_sps_pps_idr)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    mp4_h26x_writer_t wr;
    mp4_h26x_write_init(&wr, mux, 320, 240, 0);

    int sps_len, pps_len, idr_len, slice_len;
    uint8_t *sps = make_nal_buf(raw_sps, sizeof(raw_sps), &sps_len);
    uint8_t *pps = make_nal_buf(raw_pps, sizeof(raw_pps), &pps_len);
    uint8_t *idr = make_nal_buf(raw_idr, sizeof(raw_idr), &idr_len);
    uint8_t *slice = make_nal_buf(raw_slice, sizeof(raw_slice), &slice_len);

    ASSERT_EQ(mp4_h26x_write_nal(&wr, sps, sps_len, 3000), MP4E_STATUS_OK);
    ASSERT_EQ(mp4_h26x_write_nal(&wr, pps, pps_len, 3000), MP4E_STATUS_OK);
    ASSERT_EQ(mp4_h26x_write_nal(&wr, idr, idr_len, 3000), MP4E_STATUS_OK);
    ASSERT_EQ(mp4_h26x_write_nal(&wr, slice, slice_len, 3000), MP4E_STATUS_OK);

    mp4_h26x_write_close(&wr);
    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    free(sps); free(pps); free(idr); free(slice);
    free(buf.data);
}

TEST(test_h26x_write_sequential_mode)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(1, 0, &buf, write_cb);
    mp4_h26x_writer_t wr;
    mp4_h26x_write_init(&wr, mux, 320, 240, 0);

    int sps_len, pps_len, idr_len;
    uint8_t *sps = make_nal_buf(raw_sps, sizeof(raw_sps), &sps_len);
    uint8_t *pps = make_nal_buf(raw_pps, sizeof(raw_pps), &pps_len);
    uint8_t *idr = make_nal_buf(raw_idr, sizeof(raw_idr), &idr_len);

    ASSERT_EQ(mp4_h26x_write_nal(&wr, sps, sps_len, 3000), MP4E_STATUS_OK);
    ASSERT_EQ(mp4_h26x_write_nal(&wr, pps, pps_len, 3000), MP4E_STATUS_OK);
    ASSERT_EQ(mp4_h26x_write_nal(&wr, idr, idr_len, 3000), MP4E_STATUS_OK);

    mp4_h26x_write_close(&wr);
    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    free(sps); free(pps); free(idr);
    free(buf.data);
}

TEST(test_h26x_write_fragmented_mode)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 1, &buf, write_cb);
    mp4_h26x_writer_t wr;
    mp4_h26x_write_init(&wr, mux, 320, 240, 0);

    int sps_len, pps_len, idr_len, slice_len;
    uint8_t *sps = make_nal_buf(raw_sps, sizeof(raw_sps), &sps_len);
    uint8_t *pps = make_nal_buf(raw_pps, sizeof(raw_pps), &pps_len);
    uint8_t *idr = make_nal_buf(raw_idr, sizeof(raw_idr), &idr_len);
    uint8_t *slice = make_nal_buf(raw_slice, sizeof(raw_slice), &slice_len);

    ASSERT_EQ(mp4_h26x_write_nal(&wr, sps, sps_len, 3000), MP4E_STATUS_OK);
    ASSERT_EQ(mp4_h26x_write_nal(&wr, pps, pps_len, 3000), MP4E_STATUS_OK);
    ASSERT_EQ(mp4_h26x_write_nal(&wr, idr, idr_len, 3000), MP4E_STATUS_OK);

    for (int i = 0; i < 5; i++)
        ASSERT_EQ(mp4_h26x_write_nal(&wr, slice, slice_len, 3000), MP4E_STATUS_OK);

    mp4_h26x_write_close(&wr);
    MP4E_close(mux);
    ASSERT_GT(buf.size, 0);
    free(sps); free(pps); free(idr); free(slice);
    free(buf.data);
}

TEST(test_h26x_write_close_frees_memory)
{
    mem_buffer_t buf = {0};
    MP4E_mux_t *mux = MP4E_open(0, 0, &buf, write_cb);
    mp4_h26x_writer_t wr;
    mp4_h26x_write_init(&wr, mux, 320, 240, 0);

    /* Just init and close without writing any NALs */
    mp4_h26x_write_close(&wr);
    MP4E_close(mux);
    free(buf.data);
    /* ASan will catch any leaks */
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_h26x_writer\n");
    RUN_TEST(test_h26x_write_init_avc);
    RUN_TEST(test_h26x_write_init_hevc);
    RUN_TEST(test_h26x_write_sps_pps_idr);
    RUN_TEST(test_h26x_write_sequential_mode);
    RUN_TEST(test_h26x_write_fragmented_mode);
    RUN_TEST(test_h26x_write_close_frees_memory);
    TEST_SUMMARY();
}
