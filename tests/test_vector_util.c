/*
 * Tests for minimp4_vector_t internal data structure.
 */
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#include "test_harness.h"

/* ─── Tests ───────────────────────────────────────────────── */

TEST(test_vector_init_zero)
{
    minimp4_vector_t v;
    int rc = minimp4_vector_init(&v, 0);
    ASSERT_EQ(rc, 1);
    ASSERT_EQ(v.bytes, 0);
    ASSERT_EQ(v.capacity, 0);
    ASSERT_NULL(v.data);
    minimp4_vector_reset(&v);
}

TEST(test_vector_init_nonzero)
{
    minimp4_vector_t v;
    int rc = minimp4_vector_init(&v, 256);
    ASSERT_EQ(rc, 1);
    ASSERT_EQ(v.bytes, 0);
    ASSERT_EQ(v.capacity, 256);
    ASSERT_NOT_NULL(v.data);
    minimp4_vector_reset(&v);
}

TEST(test_vector_reset)
{
    minimp4_vector_t v;
    minimp4_vector_init(&v, 128);
    ASSERT_NOT_NULL(v.data);

    minimp4_vector_reset(&v);
    ASSERT_NULL(v.data);
    ASSERT_EQ(v.bytes, 0);
    ASSERT_EQ(v.capacity, 0);
}

TEST(test_vector_alloc_tail)
{
    minimp4_vector_t v;
    memset(&v, 0, sizeof(v));

    unsigned char *p = minimp4_vector_alloc_tail(&v, 10);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(v.bytes, 10);
    ASSERT_GE(v.capacity, 10);

    /* Allocate more */
    unsigned char *p2 = minimp4_vector_alloc_tail(&v, 20);
    ASSERT_NOT_NULL(p2);
    ASSERT_EQ(v.bytes, 30);
    ASSERT_EQ(p2, v.data + 10);

    minimp4_vector_reset(&v);
}

TEST(test_vector_put)
{
    minimp4_vector_t v;
    memset(&v, 0, sizeof(v));

    uint8_t data1[] = { 0xAA, 0xBB, 0xCC };
    uint8_t data2[] = { 0xDD, 0xEE };

    unsigned char *p1 = minimp4_vector_put(&v, data1, sizeof(data1));
    ASSERT_NOT_NULL(p1);
    ASSERT_EQ(v.bytes, 3);
    ASSERT_MEM_EQ(v.data, data1, 3);

    unsigned char *p2 = minimp4_vector_put(&v, data2, sizeof(data2));
    ASSERT_NOT_NULL(p2);
    ASSERT_EQ(v.bytes, 5);
    ASSERT_MEM_EQ(v.data + 3, data2, 2);

    minimp4_vector_reset(&v);
}

TEST(test_vector_grow)
{
    minimp4_vector_t v;
    minimp4_vector_init(&v, 8);

    /* Fill to capacity */
    v.bytes = 8;

    /* Grow should expand */
    int rc = minimp4_vector_grow(&v, 100);
    ASSERT_EQ(rc, 1);
    ASSERT_GE(v.capacity, 108);

    minimp4_vector_reset(&v);
}

TEST(test_vector_accumulate_many)
{
    minimp4_vector_t v;
    memset(&v, 0, sizeof(v));

    /* Accumulate 1000 single bytes — triggers multiple grows */
    for (int i = 0; i < 1000; i++) {
        uint8_t byte = (uint8_t)(i & 0xFF);
        ASSERT_NOT_NULL(minimp4_vector_put(&v, &byte, 1));
    }
    ASSERT_EQ(v.bytes, 1000);

    /* Verify data integrity */
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ(v.data[i], (uint8_t)(i & 0xFF));
    }

    minimp4_vector_reset(&v);
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void)
{
    printf("test_vector_util\n");
    RUN_TEST(test_vector_init_zero);
    RUN_TEST(test_vector_init_nonzero);
    RUN_TEST(test_vector_reset);
    RUN_TEST(test_vector_alloc_tail);
    RUN_TEST(test_vector_put);
    RUN_TEST(test_vector_grow);
    RUN_TEST(test_vector_accumulate_many);
    TEST_SUMMARY();
}
