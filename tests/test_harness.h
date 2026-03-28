/*
 * Minimal test harness for minimp4
 * Zero dependencies — only C standard library.
 *
 * Usage:
 *   #include "test_harness.h"
 *
 *   TEST(my_test) {
 *       ASSERT_EQ(1 + 1, 2);
 *       ASSERT_NOT_NULL(ptr);
 *   }
 *
 *   int main(void) {
 *       RUN_TEST(my_test);
 *       TEST_SUMMARY();
 *   }
 */
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test_total = 0;
static int test_passed = 0;
static int test_failed = 0;
static int test_current_failed = 0;

#define TEST(name) static void name(void)

#define ASSERT_EQ(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { \
        fprintf(stderr, "    FAIL %s:%d: %s == %s (got %lld vs %lld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_NE(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a == _b) { \
        fprintf(stderr, "    FAIL %s:%d: %s != %s (both are %lld)\n", \
                __FILE__, __LINE__, #a, #b, _a); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "    FAIL %s:%d: %s is false\n", \
                __FILE__, __LINE__, #expr); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(expr) do { \
    if ((expr)) { \
        fprintf(stderr, "    FAIL %s:%d: %s is true\n", \
                __FILE__, __LINE__, #expr); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "    FAIL %s:%d: %s is not NULL\n", \
                __FILE__, __LINE__, #ptr); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "    FAIL %s:%d: %s is NULL\n", \
                __FILE__, __LINE__, #ptr); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_MEM_EQ(a, b, n) do { \
    if (memcmp((a), (b), (n)) != 0) { \
        fprintf(stderr, "    FAIL %s:%d: memcmp(%s, %s, %d) != 0\n", \
                __FILE__, __LINE__, #a, #b, (int)(n)); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_GT(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (!(_a > _b)) { \
        fprintf(stderr, "    FAIL %s:%d: %s > %s (got %lld vs %lld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_GE(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (!(_a >= _b)) { \
        fprintf(stderr, "    FAIL %s:%d: %s >= %s (got %lld vs %lld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        test_current_failed = 1; \
        return; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    test_current_failed = 0; \
    test_total++; \
    fn(); \
    if (test_current_failed) { \
        test_failed++; \
        fprintf(stderr, "  FAIL  %s\n", #fn); \
    } else { \
        test_passed++; \
        printf("  PASS  %s\n", #fn); \
    } \
} while(0)

#define TEST_SUMMARY() do { \
    printf("\n%d tests: %d passed, %d failed\n", test_total, test_passed, test_failed); \
    return test_failed ? 1 : 0; \
} while(0)

#endif /* TEST_HARNESS_H */
