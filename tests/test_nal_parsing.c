/*
 * Tests for NAL unit parsing: find_nal_unit, remove_nal_escapes, nal_put_esc
 * These are static functions in minimp4.h, so we include the implementation.
 */
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#include "test_harness.h"

/* ─── find_nal_unit tests ─────────────────────────────────── */

TEST(test_find_nal_3byte_start_code)
{
    /* 00 00 01 <NAL> */
    uint8_t data[] = { 0x00, 0x00, 0x01, 0x65, 0xAA, 0xBB };
    int nal_bytes = 0;
    const uint8_t *nal = find_nal_unit(data, sizeof(data), &nal_bytes);
    ASSERT_NOT_NULL(nal);
    ASSERT_GT(nal_bytes, 0);
    /* NAL should start at the byte after start code */
    ASSERT_EQ(nal[0], 0x65);
}

TEST(test_find_nal_4byte_start_code)
{
    /* 00 00 00 01 <NAL> */
    uint8_t data[] = { 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x0A };
    int nal_bytes = 0;
    const uint8_t *nal = find_nal_unit(data, sizeof(data), &nal_bytes);
    ASSERT_NOT_NULL(nal);
    ASSERT_GT(nal_bytes, 0);
    ASSERT_EQ(nal[0], 0x67);
}

TEST(test_find_nal_two_nals)
{
    /* Two NALs separated by start codes */
    uint8_t data[] = {
        0x00, 0x00, 0x01, 0x67, 0x42,          /* NAL 1: SPS */
        0x00, 0x00, 0x01, 0x68, 0xCE, 0x38     /* NAL 2: PPS */
    };
    int nal_bytes = 0;
    const uint8_t *nal = find_nal_unit(data, sizeof(data), &nal_bytes);
    ASSERT_NOT_NULL(nal);
    /* First NAL should be 0x67 0x42 */
    ASSERT_EQ(nal[0], 0x67);
    ASSERT_EQ(nal_bytes, 2);  /* 0x67 0x42 before next start code */
}

TEST(test_find_nal_no_start_code)
{
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    int nal_bytes = 0;
    const uint8_t *nal = find_nal_unit(data, sizeof(data), &nal_bytes);
    /* No start code → find_start_code returns NULL → nal is NULL */
    /* Or it may return start==stop with nal_bytes==0 */
    if (nal != NULL) {
        ASSERT_EQ(nal_bytes, 0);
    }
}

TEST(test_find_nal_empty)
{
    /* With 0 bytes, find_nal_unit may still scan a few bytes if the
       implementation doesn't check size first. Use a padded buffer. */
    uint8_t dummy[16];
    memset(dummy, 0xFF, sizeof(dummy));  /* no start codes */
    int nal_bytes = 0;
    const uint8_t *nal = find_nal_unit(dummy, 0, &nal_bytes);
    /* Either NULL or nal_bytes==0 is acceptable */
    if (nal != NULL)
        ASSERT_EQ(nal_bytes, 0);
}

/* ─── remove_nal_escapes tests ────────────────────────────── */

TEST(test_remove_escapes_no_escapes)
{
    uint8_t src[] = { 0x67, 0x42, 0x00, 0x0A, 0xF8 };
    uint8_t dst[16];
    int n = remove_nal_escapes(dst, src, sizeof(src));
    ASSERT_EQ(n, (int)sizeof(src));
    ASSERT_MEM_EQ(dst, src, sizeof(src));
}

TEST(test_remove_escapes_with_escape)
{
    /* 00 00 03 01 → escape byte 03 should be removed → 00 00 01 */
    uint8_t src[] = { 0x00, 0x00, 0x03, 0x01, 0xFF };
    uint8_t expected[] = { 0x00, 0x00, 0x01, 0xFF };
    uint8_t dst[16];
    int n = remove_nal_escapes(dst, src, sizeof(src));
    ASSERT_EQ(n, (int)sizeof(expected));
    ASSERT_MEM_EQ(dst, expected, sizeof(expected));
}

TEST(test_remove_escapes_multiple)
{
    /* 00 00 03 02 → removes 03 → 00 00 02
       Then 00 00 03 01 → removes 03 → 00 00 01 */
    uint8_t src[] = { 0x00, 0x00, 0x03, 0x02, 0x00, 0x00, 0x03, 0x01 };
    uint8_t expected[] = { 0x00, 0x00, 0x02, 0x00, 0x00, 0x01 };
    uint8_t dst[16];
    int n = remove_nal_escapes(dst, src, sizeof(src));
    ASSERT_EQ(n, (int)sizeof(expected));
    ASSERT_MEM_EQ(dst, expected, sizeof(expected));
}

/* ─── nal_put_esc tests ───────────────────────────────────── */

TEST(test_put_esc_no_escapes_needed)
{
    uint8_t src[] = { 0x67, 0x42, 0x0A };
    uint8_t dst[32];
    int n = nal_put_esc(dst, src, sizeof(src));
    /* Output starts with 00 00 00 01 start code */
    ASSERT_EQ(dst[0], 0x00);
    ASSERT_EQ(dst[1], 0x00);
    ASSERT_EQ(dst[2], 0x00);
    ASSERT_EQ(dst[3], 0x01);
    /* Then the original data */
    ASSERT_EQ(n, 4 + (int)sizeof(src));
    ASSERT_MEM_EQ(dst + 4, src, sizeof(src));
}

TEST(test_put_esc_needs_escape)
{
    /* Input with 00 00 01 sequence requires escape insertion */
    uint8_t src[] = { 0x00, 0x00, 0x01 };
    uint8_t dst[32];
    int n = nal_put_esc(dst, src, sizeof(src));
    /* Should be: 00 00 00 01 | 00 00 03 01 */
    ASSERT_EQ(n, 4 + 4); /* start code + escaped data */
    ASSERT_EQ(dst[4], 0x00);
    ASSERT_EQ(dst[5], 0x00);
    ASSERT_EQ(dst[6], 0x03); /* escape byte inserted */
    ASSERT_EQ(dst[7], 0x01);
}

TEST(test_put_esc_roundtrip)
{
    /* Verify: remove_nal_escapes(nal_put_esc(data)) == data */
    uint8_t original[] = { 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0xFF };
    uint8_t escaped[64];
    int esc_len = nal_put_esc(escaped, original, sizeof(original));

    uint8_t recovered[64];
    int rec_len = remove_nal_escapes(recovered, escaped + 4, esc_len - 4);
    ASSERT_EQ(rec_len, (int)sizeof(original));
    ASSERT_MEM_EQ(recovered, original, sizeof(original));
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_nal_parsing\n");
    RUN_TEST(test_find_nal_3byte_start_code);
    RUN_TEST(test_find_nal_4byte_start_code);
    RUN_TEST(test_find_nal_two_nals);
    RUN_TEST(test_find_nal_no_start_code);
    RUN_TEST(test_find_nal_empty);
    RUN_TEST(test_remove_escapes_no_escapes);
    RUN_TEST(test_remove_escapes_with_escape);
    RUN_TEST(test_remove_escapes_multiple);
    RUN_TEST(test_put_esc_no_escapes_needed);
    RUN_TEST(test_put_esc_needs_escape);
    RUN_TEST(test_put_esc_roundtrip);
    TEST_SUMMARY();
}
