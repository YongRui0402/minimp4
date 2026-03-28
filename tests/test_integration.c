/*
 * Integration tests: round-trip mux → demux using real H.264 test vectors.
 * Replaces the shell-based cmp tests in scripts/test.sh.
 */
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#include "test_harness.h"

/* ─── I/O helpers ─────────────────────────────────────────── */

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t size;
} mem_writer_t;

static int write_cb(int64_t offset, const void *buffer, size_t size, void *token)
{
    mem_writer_t *w = (mem_writer_t *)token;
    size_t needed = (size_t)offset + size;
    if (needed > w->capacity) {
        size_t cap = w->capacity ? w->capacity : 4096;
        while (cap < needed) cap *= 2;
        uint8_t *tmp = (uint8_t *)realloc(w->data, cap);
        if (!tmp) return 1;
        w->data = tmp;
        w->capacity = cap;
    }
    memcpy(w->data + offset, buffer, size);
    if (needed > w->size) w->size = needed;
    return 0;
}

typedef struct {
    const uint8_t *data;
    size_t size;
} mem_reader_t;

static int read_cb(int64_t offset, void *buffer, size_t size, void *token)
{
    mem_reader_t *r = (mem_reader_t *)token;
    if ((size_t)offset + size > r->size) return 1;
    memcpy(buffer, r->data + offset, size);
    return 0;
}

static uint8_t *load_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *data = (uint8_t *)malloc((size_t)sz);
    if (!data) { fclose(f); return NULL; }
    if ((long)fread(data, 1, (size_t)sz, f) != sz) { free(data); fclose(f); return NULL; }
    fclose(f);
    *out_size = (size_t)sz;
    return data;
}

/* Find next NAL unit boundary (same logic as minimp4_test.c) */
static size_t find_nal_boundary(const uint8_t *buf, size_t size)
{
    size_t pos = 3;
    while ((size - pos) > 3) {
        if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 1)
            return pos;
        if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 0 && buf[pos + 3] == 1)
            return pos;
        pos++;
    }
    return size;
}

/* Mux an H.264 elementary stream into MP4 in memory */
static int mux_h264(const uint8_t *h264, size_t h264_size, mem_writer_t *mp4,
                    int sequential, int fragmented)
{
    memset(mp4, 0, sizeof(*mp4));
    MP4E_mux_t *mux = MP4E_open(sequential, fragmented, mp4, write_cb);
    if (!mux) return -1;

    mp4_h26x_writer_t wr;
    if (MP4E_STATUS_OK != mp4_h26x_write_init(&wr, mux, 352, 288, 0)) {
        MP4E_close(mux);
        return -1;
    }

    const uint8_t *p = h264;
    size_t remain = h264_size;
    while (remain > 0) {
        size_t nal_size = find_nal_boundary(p, remain);
        if (nal_size < 4) { p += 1; remain -= 1; continue; }
        if (MP4E_STATUS_OK != mp4_h26x_write_nal(&wr, p, nal_size, 90000 / 30)) {
            mp4_h26x_write_close(&wr);
            MP4E_close(mux);
            return -1;
        }
        p += nal_size;
        remain -= nal_size;
    }

    MP4E_close(mux);
    mp4_h26x_write_close(&wr);
    return 0;
}

/* Demux an MP4 back to H.264 elementary stream in memory */
static int demux_to_h264(const uint8_t *mp4_data, size_t mp4_size, mem_writer_t *h264_out)
{
    memset(h264_out, 0, sizeof(*h264_out));
    mem_reader_t r = { mp4_data, mp4_size };
    MP4D_demux_t mp4;
    memset(&mp4, 0, sizeof(mp4));

    if (!MP4D_open(&mp4, read_cb, &r, (int64_t)mp4_size))
        return -1;

    /* Find video track */
    int vtrack = -1;
    for (unsigned t = 0; t < mp4.track_count; t++) {
        if (mp4.track[t].handler_type == MP4D_HANDLER_TYPE_VIDE) {
            vtrack = (int)t;
            break;
        }
    }
    if (vtrack < 0) { MP4D_close(&mp4); return -1; }

    uint8_t sync[4] = { 0, 0, 0, 1 };

    /* Write SPS */
    int i = 0, nbytes;
    const void *data;
    while ((data = MP4D_read_sps(&mp4, vtrack, i++, &nbytes)) != NULL) {
        write_cb((int64_t)h264_out->size, sync, 4, h264_out);
        write_cb((int64_t)h264_out->size, data, nbytes, h264_out);
    }
    /* Write PPS */
    i = 0;
    while ((data = MP4D_read_pps(&mp4, vtrack, i++, &nbytes)) != NULL) {
        write_cb((int64_t)h264_out->size, sync, 4, h264_out);
        write_cb((int64_t)h264_out->size, data, nbytes, h264_out);
    }
    /* Write frames */
    for (unsigned s = 0; s < mp4.track[vtrack].sample_count; s++) {
        unsigned frame_bytes, timestamp, duration;
        MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, vtrack, s,
                                                    &frame_bytes, &timestamp, &duration);
        const uint8_t *mem = mp4_data + ofs;
        while (frame_bytes > 0) {
            uint32_t size = ((uint32_t)mem[0] << 24) | ((uint32_t)mem[1] << 16) |
                            ((uint32_t)mem[2] << 8) | mem[3];
            size += 4;
            if (frame_bytes < size) break;
            /* Write start code + NAL data */
            write_cb((int64_t)h264_out->size, sync, 4, h264_out);
            write_cb((int64_t)h264_out->size, mem + 4, size - 4, h264_out);
            frame_bytes -= size;
            mem += size;
        }
    }

    MP4D_close(&mp4);
    return 0;
}

/* ─── Round-trip test helper ──────────────────────────────── */

/* Mux h264 → mp4, compare mp4 with reference */
static int test_mux_against_ref(const char *input_path, const char *ref_path,
                                int sequential, int fragmented)
{
    size_t h264_size = 0;
    uint8_t *h264 = load_file(input_path, &h264_size);
    if (!h264) return -1;

    mem_writer_t mp4;
    int rc = mux_h264(h264, h264_size, &mp4, sequential, fragmented);
    free(h264);
    if (rc != 0) return -1;

    size_t ref_size = 0;
    uint8_t *ref = load_file(ref_path, &ref_size);
    if (!ref) { free(mp4.data); return -1; }

    int match = (mp4.size == ref_size) && (memcmp(mp4.data, ref, ref_size) == 0);
    free(mp4.data);
    free(ref);
    return match ? 0 : -1;
}

/* Demux mp4 → h264, compare with reference elementary stream */
static int test_demux_against_ref(const char *mp4_path, const char *ref_h264_path)
{
    size_t mp4_size = 0;
    uint8_t *mp4_data = load_file(mp4_path, &mp4_size);
    if (!mp4_data) return -1;

    mem_writer_t h264_out;
    int rc = demux_to_h264(mp4_data, mp4_size, &h264_out);
    free(mp4_data);
    if (rc != 0) return -1;

    size_t ref_size = 0;
    uint8_t *ref = load_file(ref_h264_path, &ref_size);
    if (!ref) { free(h264_out.data); return -1; }

    int match = (h264_out.size == ref_size) && (memcmp(h264_out.data, ref, ref_size) == 0);
    free(h264_out.data);
    free(ref);
    return match ? 0 : -1;
}

/* ─── Tests ───────────────────────────────────────────────── */

TEST(test_mux_default)
{
    ASSERT_EQ(test_mux_against_ref("vectors/foreman.264", "vectors/out_ref.mp4", 0, 0), 0);
}

TEST(test_mux_sequential)
{
    ASSERT_EQ(test_mux_against_ref("vectors/foreman.264", "vectors/out_sequential_ref.mp4", 1, 0), 0);
}

TEST(test_mux_fragmented)
{
    ASSERT_EQ(test_mux_against_ref("vectors/foreman.264", "vectors/out_fragmentation_ref.mp4", 0, 1), 0);
}

TEST(test_mux_slices)
{
    ASSERT_EQ(test_mux_against_ref("vectors/foreman_slices.264", "vectors/out_slices_ref.mp4", 0, 0), 0);
}

TEST(test_demux_default)
{
    ASSERT_EQ(test_demux_against_ref("vectors/out_ref.mp4", "vectors/foreman.264"), 0);
}

TEST(test_demux_sequential)
{
    ASSERT_EQ(test_demux_against_ref("vectors/out_sequential_ref.mp4", "vectors/foreman.264"), 0);
}

TEST(test_demux_slices)
{
    ASSERT_EQ(test_demux_against_ref("vectors/out_slices_ref.mp4", "vectors/foreman_slices.264"), 0);
}

TEST(test_roundtrip_default)
{
    /* mux → demux → compare with original */
    size_t h264_size = 0;
    uint8_t *h264 = load_file("vectors/foreman.264", &h264_size);
    ASSERT_NOT_NULL(h264);

    mem_writer_t mp4;
    ASSERT_EQ(mux_h264(h264, h264_size, &mp4, 0, 0), 0);

    mem_writer_t h264_out;
    ASSERT_EQ(demux_to_h264(mp4.data, mp4.size, &h264_out), 0);

    ASSERT_EQ(h264_out.size, h264_size);
    ASSERT_EQ(memcmp(h264_out.data, h264, h264_size), 0);

    free(h264);
    free(mp4.data);
    free(h264_out.data);
}

TEST(test_roundtrip_sequential)
{
    size_t h264_size = 0;
    uint8_t *h264 = load_file("vectors/foreman.264", &h264_size);
    ASSERT_NOT_NULL(h264);

    mem_writer_t mp4;
    ASSERT_EQ(mux_h264(h264, h264_size, &mp4, 1, 0), 0);

    mem_writer_t h264_out;
    ASSERT_EQ(demux_to_h264(mp4.data, mp4.size, &h264_out), 0);

    ASSERT_EQ(h264_out.size, h264_size);
    ASSERT_EQ(memcmp(h264_out.data, h264, h264_size), 0);

    free(h264);
    free(mp4.data);
    free(h264_out.data);
}

TEST(test_roundtrip_slices)
{
    size_t h264_size = 0;
    uint8_t *h264 = load_file("vectors/foreman_slices.264", &h264_size);
    ASSERT_NOT_NULL(h264);

    mem_writer_t mp4;
    ASSERT_EQ(mux_h264(h264, h264_size, &mp4, 0, 0), 0);

    mem_writer_t h264_out;
    ASSERT_EQ(demux_to_h264(mp4.data, mp4.size, &h264_out), 0);

    ASSERT_EQ(h264_out.size, h264_size);
    ASSERT_EQ(memcmp(h264_out.data, h264, h264_size), 0);

    free(h264);
    free(mp4.data);
    free(h264_out.data);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_integration\n");
    /* Mux against reference */
    RUN_TEST(test_mux_default);
    RUN_TEST(test_mux_sequential);
    RUN_TEST(test_mux_fragmented);
    RUN_TEST(test_mux_slices);
    /* Demux against reference */
    RUN_TEST(test_demux_default);
    RUN_TEST(test_demux_sequential);
    RUN_TEST(test_demux_slices);
    /* Full round-trip */
    RUN_TEST(test_roundtrip_default);
    RUN_TEST(test_roundtrip_sequential);
    RUN_TEST(test_roundtrip_slices);
    TEST_SUMMARY();
}
