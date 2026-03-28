/*
 * Tests for bit_reader_t and bs_t (bitstream reader/writer) in minimp4.h.
 * These are static functions, so we include the implementation.
 */
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#include "test_harness.h"

/* ─── bit_reader_t tests ──────────────────────────────────── */

/*
 * Note: bit_reader_t internally uses uint16_t* pointers, so data buffers
 * must be at least 4 bytes and ideally padded to avoid reading past the end.
 */

TEST(test_init_bits)
{
    uint8_t data[] = { 0xFF, 0x00, 0xAA, 0x55, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);
    ASSERT_EQ(remaining_bits(&bs), 32);
    ASSERT_EQ(get_pos_bits(&bs), 0);
}

TEST(test_get_bits_byte_aligned)
{
    uint8_t data[] = { 0xAB, 0xCD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);

    unsigned val = get_bits(&bs, 8);
    ASSERT_EQ(val, 0xAB);

    val = get_bits(&bs, 8);
    ASSERT_EQ(val, 0xCD);
}

TEST(test_get_bits_partial)
{
    /* 0xA5 = 1010 0101 */
    uint8_t data[] = { 0xA5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);

    unsigned val = get_bits(&bs, 4);
    ASSERT_EQ(val, 0xA); /* 1010 */

    val = get_bits(&bs, 4);
    ASSERT_EQ(val, 0x5); /* 0101 */
}

TEST(test_show_bits_no_consume)
{
    uint8_t data[] = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);

    unsigned val1 = show_bits(&bs, 8);
    unsigned val2 = show_bits(&bs, 8);
    ASSERT_EQ(val1, val2);
    ASSERT_EQ(val1, 0xFF);
    ASSERT_EQ(get_pos_bits(&bs), 0);
}

TEST(test_flush_bits)
{
    uint8_t data[] = { 0xAB, 0xCD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);

    flush_bits(&bs, 8);
    ASSERT_EQ(get_pos_bits(&bs), 8);

    unsigned val = get_bits(&bs, 8);
    ASSERT_EQ(val, 0xCD);
}

TEST(test_remaining_bits)
{
    uint8_t data[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);

    ASSERT_EQ(remaining_bits(&bs), 32);
    get_bits(&bs, 5);
    ASSERT_EQ(remaining_bits(&bs), 27);
    get_bits(&bs, 11);
    ASSERT_EQ(remaining_bits(&bs), 16);
}

TEST(test_set_pos_bits)
{
    uint8_t data[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);

    set_pos_bits(&bs, 16);
    ASSERT_EQ(get_pos_bits(&bs), 16);
    unsigned val = get_bits(&bs, 8);
    ASSERT_EQ(val, 0xCC);
}

TEST(test_ue_bits_zero)
{
    /* Exp-Golomb code for 0: binary "1" → 0x80 */
    uint8_t data[] = { 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);
    int val = ue_bits(&bs);
    ASSERT_EQ(val, 0);
}

TEST(test_ue_bits_one)
{
    /* Exp-Golomb code for 1: binary "010" → 0x40 */
    uint8_t data[] = { 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);
    int val = ue_bits(&bs);
    ASSERT_EQ(val, 1);
}

TEST(test_ue_bits_two)
{
    /* Exp-Golomb code for 2: binary "011" → 0x60 */
    uint8_t data[] = { 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);
    int val = ue_bits(&bs);
    ASSERT_EQ(val, 2);
}

TEST(test_ue_bits_seven)
{
    /* Exp-Golomb code for 7: N+1=8=0b1000, codeword="0001000" (7 bits)
       In byte: 0001 0000 = 0x10 */
    uint8_t data[] = { 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    bit_reader_t bs;
    init_bits(&bs, data, 4);
    int val = ue_bits(&bs);
    ASSERT_EQ(val, 7);
}

/* ─── bs_t (bitstream writer) tests ───────────────────────── */

/*
 * bs_t writer uses uint32_t* (bs_item_t) internally and writes in big-endian
 * via SWAP32. Must call h264e_bs_flush() before reading back.
 * Use heap-allocated aligned buffers to avoid ASan stack overflow.
 */

TEST(test_bs_write_and_read)
{
    uint8_t *buf = (uint8_t *)calloc(1, 256);
    bs_t wr;
    h264e_bs_init_bits(&wr, buf);

    h264e_bs_put_bits(&wr, 8, 0xAB);
    h264e_bs_put_bits(&wr, 8, 0xCD);
    h264e_bs_flush(&wr);

    bit_reader_t rd;
    init_bits(&rd, buf, 256);
    ASSERT_EQ(get_bits(&rd, 8), 0xAB);
    ASSERT_EQ(get_bits(&rd, 8), 0xCD);
    free(buf);
}

TEST(test_bs_write_partial_bits)
{
    uint8_t *buf = (uint8_t *)calloc(1, 256);
    bs_t wr;
    h264e_bs_init_bits(&wr, buf);

    h264e_bs_put_bits(&wr, 4, 0xA);
    h264e_bs_put_bits(&wr, 4, 0x5);
    h264e_bs_flush(&wr);

    bit_reader_t rd;
    init_bits(&rd, buf, 256);
    ASSERT_EQ(get_bits(&rd, 8), 0xA5);
    free(buf);
}

TEST(test_bs_put_golomb)
{
    uint8_t *buf = (uint8_t *)calloc(1, 256);
    bs_t wr;
    h264e_bs_init_bits(&wr, buf);

    h264e_bs_put_golomb(&wr, 0);
    h264e_bs_flush(&wr);

    bit_reader_t rd;
    init_bits(&rd, buf, 256);
    ASSERT_EQ(ue_bits(&rd), 0);
    free(buf);
}

TEST(test_bs_put_golomb_roundtrip)
{
    uint8_t *buf = (uint8_t *)calloc(1, 512);
    bs_t wr;
    h264e_bs_init_bits(&wr, buf);

    int values[] = { 0, 1, 2, 3, 5, 10, 100 };
    int n = sizeof(values) / sizeof(values[0]);
    for (int i = 0; i < n; i++)
        h264e_bs_put_golomb(&wr, values[i]);
    h264e_bs_flush(&wr);

    bit_reader_t rd;
    init_bits(&rd, buf, 512);
    for (int i = 0; i < n; i++)
        ASSERT_EQ(ue_bits(&rd), values[i]);
    free(buf);
}

TEST(test_bs_get_pos_bits)
{
    uint8_t *buf = (uint8_t *)calloc(1, 256);
    bs_t wr;
    h264e_bs_init_bits(&wr, buf);

    unsigned pos0 = h264e_bs_get_pos_bits(&wr);
    h264e_bs_put_bits(&wr, 16, 0x1234);
    unsigned pos1 = h264e_bs_get_pos_bits(&wr);
    ASSERT_EQ(pos1 - pos0, 16);
    free(buf);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_bit_reader\n");
    /* bit_reader_t */
    RUN_TEST(test_init_bits);
    RUN_TEST(test_get_bits_byte_aligned);
    RUN_TEST(test_get_bits_partial);
    RUN_TEST(test_show_bits_no_consume);
    RUN_TEST(test_flush_bits);
    RUN_TEST(test_remaining_bits);
    RUN_TEST(test_set_pos_bits);
    RUN_TEST(test_ue_bits_zero);
    RUN_TEST(test_ue_bits_one);
    RUN_TEST(test_ue_bits_two);
    RUN_TEST(test_ue_bits_seven);
    /* bs_t */
    RUN_TEST(test_bs_write_and_read);
    RUN_TEST(test_bs_write_partial_bits);
    RUN_TEST(test_bs_put_golomb);
    RUN_TEST(test_bs_put_golomb_roundtrip);
    RUN_TEST(test_bs_get_pos_bits);
    TEST_SUMMARY();
}
