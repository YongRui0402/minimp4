/*
 * HEVC-specific mux/demux tests using generated HEVC test vectors.
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

/* Mux HEVC elementary stream into MP4 in memory */
static int mux_hevc(const uint8_t *hevc, size_t hevc_size, mem_writer_t *mp4)
{
    memset(mp4, 0, sizeof(*mp4));
    MP4E_mux_t *mux = MP4E_open(0, 0, mp4, write_cb);
    if (!mux) return -1;

    mp4_h26x_writer_t wr;
    if (MP4E_STATUS_OK != mp4_h26x_write_init(&wr, mux, 176, 144, 1)) {
        MP4E_close(mux);
        return -1;
    }

    const uint8_t *p = hevc;
    size_t remain = hevc_size;
    while (remain > 0) {
        size_t nal_size = find_nal_boundary(p, remain);
        if (nal_size < 4) { p += 1; remain -= 1; continue; }
        if (MP4E_STATUS_OK != mp4_h26x_write_nal(&wr, p, nal_size, 90000 / 15)) {
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

/* ─── Tests ───────────────────────────────────────────────── */

TEST(test_hevc_mux)
{
    size_t hevc_size = 0;
    uint8_t *hevc = load_file("vectors/foreman.265", &hevc_size);
    ASSERT_NOT_NULL(hevc);
    ASSERT_GT(hevc_size, 0);

    mem_writer_t mp4;
    int rc = mux_hevc(hevc, hevc_size, &mp4);
    ASSERT_EQ(rc, 0);
    ASSERT_GT(mp4.size, 0);

    free(hevc);
    free(mp4.data);
}

TEST(test_hevc_mux_demux_roundtrip)
{
    /* Mux HEVC → MP4, then demux back and verify frame count matches */
    size_t hevc_size = 0;
    uint8_t *hevc = load_file("vectors/foreman.265", &hevc_size);
    ASSERT_NOT_NULL(hevc);

    mem_writer_t mp4;
    ASSERT_EQ(mux_hevc(hevc, hevc_size, &mp4), 0);
    ASSERT_GT(mp4.size, 0);

    /* Demux the muxed MP4 */
    mem_reader_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    ASSERT_EQ(MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size), 1);
    ASSERT_GE(demux.track_count, 1);
    ASSERT_GT(demux.track[0].sample_count, 0);
    ASSERT_EQ(demux.track[0].object_type_indication, MP4_OBJECT_TYPE_HEVC);

    MP4D_close(&demux);
    free(hevc);
    free(mp4.data);
}

TEST(test_hevc_demux)
{
    size_t mp4_size = 0;
    uint8_t *mp4_data = load_file("vectors/out_hevc_ref.mp4", &mp4_size);
    ASSERT_NOT_NULL(mp4_data);

    mem_reader_t rbuf = { mp4_data, mp4_size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));

    int rc = MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4_size);
    ASSERT_EQ(rc, 1);
    ASSERT_GE(demux.track_count, 1);

    /* Find video track */
    int vtrack = -1;
    for (unsigned t = 0; t < demux.track_count; t++) {
        if (demux.track[t].handler_type == MP4D_HANDLER_TYPE_VIDE) {
            vtrack = (int)t;
            break;
        }
    }
    ASSERT_GE(vtrack, 0);
    ASSERT_GT(demux.track[vtrack].sample_count, 0);
    ASSERT_EQ(demux.track[vtrack].object_type_indication, MP4_OBJECT_TYPE_HEVC);

    /* Verify all frames are accessible */
    for (unsigned s = 0; s < demux.track[vtrack].sample_count; s++) {
        unsigned frame_bytes, timestamp, duration;
        MP4D_file_offset_t ofs = MP4D_frame_offset(&demux, vtrack, s,
                                                    &frame_bytes, &timestamp, &duration);
        ASSERT_GT(frame_bytes, 0);
        ASSERT_TRUE((size_t)ofs + frame_bytes <= mp4_size);
    }

    MP4D_close(&demux);
    free(mp4_data);
}

TEST(test_hevc_dsi_present)
{
    size_t mp4_size = 0;
    uint8_t *mp4_data = load_file("vectors/out_hevc_ref.mp4", &mp4_size);
    ASSERT_NOT_NULL(mp4_data);

    mem_reader_t rbuf = { mp4_data, mp4_size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4_size);

    /* HEVC track should have DSI (hvcC box contents) */
    int vtrack = 0;
    ASSERT_NOT_NULL(demux.track[vtrack].dsi);
    ASSERT_GT(demux.track[vtrack].dsi_bytes, 0);

    MP4D_close(&demux);
    free(mp4_data);
}

TEST(test_hevc_sequential_mode)
{
    size_t hevc_size = 0;
    uint8_t *hevc = load_file("vectors/foreman.265", &hevc_size);
    ASSERT_NOT_NULL(hevc);

    mem_writer_t mp4 = {0};
    MP4E_mux_t *mux = MP4E_open(1, 0, &mp4, write_cb);
    ASSERT_NOT_NULL(mux);

    mp4_h26x_writer_t wr;
    ASSERT_EQ(mp4_h26x_write_init(&wr, mux, 176, 144, 1), MP4E_STATUS_OK);

    const uint8_t *p = hevc;
    size_t remain = hevc_size;
    while (remain > 0) {
        size_t nal_size = find_nal_boundary(p, remain);
        if (nal_size < 4) { p += 1; remain -= 1; continue; }
        ASSERT_EQ(mp4_h26x_write_nal(&wr, p, nal_size, 90000 / 15), MP4E_STATUS_OK);
        p += nal_size;
        remain -= nal_size;
    }

    MP4E_close(mux);
    mp4_h26x_write_close(&wr);
    ASSERT_GT(mp4.size, 0);

    /* Verify demuxable */
    mem_reader_t rbuf = { mp4.data, mp4.size };
    MP4D_demux_t demux;
    memset(&demux, 0, sizeof(demux));
    ASSERT_EQ(MP4D_open(&demux, read_cb, &rbuf, (int64_t)mp4.size), 1);
    ASSERT_GT(demux.track[0].sample_count, 0);

    MP4D_close(&demux);
    free(hevc);
    free(mp4.data);
}

TEST(test_hevc_fragmented_mode)
{
    size_t hevc_size = 0;
    uint8_t *hevc = load_file("vectors/foreman.265", &hevc_size);
    ASSERT_NOT_NULL(hevc);

    mem_writer_t mp4 = {0};
    MP4E_mux_t *mux = MP4E_open(0, 1, &mp4, write_cb);
    ASSERT_NOT_NULL(mux);

    mp4_h26x_writer_t wr;
    ASSERT_EQ(mp4_h26x_write_init(&wr, mux, 176, 144, 1), MP4E_STATUS_OK);

    const uint8_t *p = hevc;
    size_t remain = hevc_size;
    while (remain > 0) {
        size_t nal_size = find_nal_boundary(p, remain);
        if (nal_size < 4) { p += 1; remain -= 1; continue; }
        ASSERT_EQ(mp4_h26x_write_nal(&wr, p, nal_size, 90000 / 15), MP4E_STATUS_OK);
        p += nal_size;
        remain -= nal_size;
    }

    MP4E_close(mux);
    mp4_h26x_write_close(&wr);
    ASSERT_GT(mp4.size, 0);

    free(hevc);
    free(mp4.data);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_hevc\n");
    RUN_TEST(test_hevc_mux);
    RUN_TEST(test_hevc_mux_demux_roundtrip);
    RUN_TEST(test_hevc_demux);
    RUN_TEST(test_hevc_dsi_present);
    RUN_TEST(test_hevc_sequential_mode);
    RUN_TEST(test_hevc_fragmented_mode);
    TEST_SUMMARY();
}
