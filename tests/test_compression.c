/**
 * test_compression.c
 * 
 * Unit tests for Compression
 */

#include "../src/common/compression.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_START(name) \
    do { \
        printf("\n=== Test %d: %s ===\n", ++tests_run, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("✓ PASSED\n"); \
        tests_passed++; \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("✗ FAILED: %s\n", msg); \
            printf("  Line %d: %s\n", __LINE__, #cond); \
            return; \
        } \
    } while(0)

/**
 * Test 1: Compress and Decompress Simple Data
 */
void test_compress_decompress(void) {
    TEST_START("Compress and Decompress Simple Data");
    
    // Create test data
    uint8_t input[100];
    memset(input, 'A', 100);  // Highly compressible
    
    // Compress
    size_t max_compressed = compress_max_size(100);
    uint8_t *compressed = malloc(max_compressed);
    ASSERT(compressed != NULL, "Malloc should succeed");
    
    ssize_t compressed_size = compress_data(input, 100, compressed, max_compressed,
                                             COMPRESS_LEVEL_DEFAULT);
    ASSERT(compressed_size > 0, "Compression should succeed");
    ASSERT(compressed_size < 100, "Should compress repetitive data");
    
    printf("Compressed 100 bytes to %zd bytes (%d%% saved)\n", 
           compressed_size, compression_ratio(100, compressed_size));
    
    // Decompress
    uint8_t decompressed[100];
    ssize_t decompressed_size = decompress_data(compressed, compressed_size,
                                                 decompressed, 100);
    ASSERT(decompressed_size == 100, "Decompressed size should be 100");
    ASSERT(memcmp(input, decompressed, 100) == 0, "Data should match original");
    
    free(compressed);
    TEST_PASS();
}

/**
 * Test 2: Large Data Compression
 */
void test_large_data(void) {
    TEST_START("Large Data Compression");
    
    // Create large test data (10KB)
    size_t size = 10000;
    uint8_t *input = malloc(size);
    ASSERT(input != NULL, "Malloc should succeed");
    
    // Fill with pattern (compressible)
    for (size_t i = 0; i < size; i++) {
        input[i] = (i % 256);
    }
    
    // Compress
    size_t max_compressed = compress_max_size(size);
    uint8_t *compressed = malloc(max_compressed);
    ASSERT(compressed != NULL, "Malloc should succeed");
    
    ssize_t compressed_size = compress_data(input, size, compressed, max_compressed,
                                             COMPRESS_LEVEL_DEFAULT);
    ASSERT(compressed_size > 0, "Compression should succeed");
    
    printf("Compressed %zu bytes to %zd bytes (%d%% saved)\n",
           size, compressed_size, compression_ratio(size, compressed_size));
    
    // Decompress
    uint8_t *decompressed = malloc(size);
    ASSERT(decompressed != NULL, "Malloc should succeed");
    
    ssize_t decompressed_size = decompress_data(compressed, compressed_size,
                                                 decompressed, size);
    ASSERT(decompressed_size == (ssize_t)size, "Decompressed size should match");
    ASSERT(memcmp(input, decompressed, size) == 0, "Data should match");
    
    free(input);
    free(compressed);
    free(decompressed);
    TEST_PASS();
}

/**
 * Test 3: Random Data (Incompressible)
 */
void test_random_data(void) {
    TEST_START("Random Data (Incompressible)");
    
    // Create random data (not compressible)
    uint8_t input[1000];
    for (int i = 0; i < 1000; i++) {
        input[i] = (uint8_t)(rand() % 256);
    }
    
    // Compress
    size_t max_compressed = compress_max_size(1000);
    uint8_t *compressed = malloc(max_compressed);
    ASSERT(compressed != NULL, "Malloc should succeed");
    
    ssize_t compressed_size = compress_data(input, 1000, compressed, max_compressed,
                                             COMPRESS_LEVEL_DEFAULT);
    ASSERT(compressed_size > 0, "Compression should succeed");
    
    // Random data may not compress well
    printf("Random data: 1000 bytes → %zd bytes (ratio: %d%%)\n",
           compressed_size, compression_ratio(1000, compressed_size));
    
    // But should still decompress correctly
    uint8_t decompressed[1000];
    ssize_t decompressed_size = decompress_data(compressed, compressed_size,
                                                 decompressed, 1000);
    ASSERT(decompressed_size == 1000, "Should decompress to original size");
    ASSERT(memcmp(input, decompressed, 1000) == 0, "Data should match");
    
    free(compressed);
    TEST_PASS();
}

/**
 * Test 4: Compression Levels
 */
void test_compression_levels(void) {
    TEST_START("Compression Levels");
    
    // Highly compressible data
    uint8_t input[1000];
    memset(input, 'X', 1000);
    
    size_t max_compressed = compress_max_size(1000);
    uint8_t *compressed = malloc(max_compressed);
    
    // Test different levels
    ssize_t fast_size = compress_data(input, 1000, compressed, max_compressed,
                                       COMPRESS_LEVEL_FAST);
    ASSERT(fast_size > 0, "Fast compression should succeed");
    
    ssize_t default_size = compress_data(input, 1000, compressed, max_compressed,
                                          COMPRESS_LEVEL_DEFAULT);
    ASSERT(default_size > 0, "Default compression should succeed");
    
    ssize_t best_size = compress_data(input, 1000, compressed, max_compressed,
                                       COMPRESS_LEVEL_BEST);
    ASSERT(best_size > 0, "Best compression should succeed");
    
    // Best should be smallest
    ASSERT(best_size <= default_size, "Best should be <= default");
    ASSERT(default_size <= fast_size, "Default should be <= fast");
    
    printf("Compression levels:\n");
    printf("  FAST (1):    %zd bytes\n", fast_size);
    printf("  DEFAULT (6): %zd bytes\n", default_size);
    printf("  BEST (9):    %zd bytes\n", best_size);
    
    free(compressed);
    TEST_PASS();
}

/**
 * Test 5: Should Compress Helper
 */
void test_should_compress(void) {
    TEST_START("Should Compress Helper");
    
    // Very small data should not be compressed
    ASSERT(!should_compress(10), "10 bytes should not be compressed");
    ASSERT(!should_compress(50), "50 bytes should not be compressed");
    
    // Larger data should be compressed
    ASSERT(should_compress(100), "100 bytes should be compressed");
    ASSERT(should_compress(1000), "1000 bytes should be compressed");
    
    printf("Compression threshold: %d bytes\n", COMPRESS_MIN_SIZE);
    
    TEST_PASS();
}

/**
 * Test 6: Empty and Small Data
 */
void test_edge_cases(void) {
    TEST_START("Empty and Small Data");
    
    uint8_t input[10] = "SMALL";
    
    // Compress small data
    size_t max_compressed = compress_max_size(5);
    uint8_t *compressed = malloc(max_compressed);
    
    ssize_t compressed_size = compress_data(input, 5, compressed, max_compressed,
                                             COMPRESS_LEVEL_DEFAULT);
    ASSERT(compressed_size > 0, "Should handle small data");
    
    // Decompress
    uint8_t decompressed[10];
    ssize_t decompressed_size = decompress_data(compressed, compressed_size,
                                                 decompressed, 10);
    ASSERT(decompressed_size == 5, "Should decompress correctly");
    ASSERT(memcmp(input, decompressed, 5) == 0, "Data should match");
    
    printf("Small data (5 bytes) handled correctly\n");
    
    free(compressed);
    TEST_PASS();
}

/**
 * Test 7: Buffer Too Small Error
 */
void test_buffer_too_small(void) {
    TEST_START("Buffer Too Small Error");
    
    uint8_t input[1000];
    memset(input, 'A', 1000);
    
    // Try to compress into too small buffer
    uint8_t small_buffer[10];
    ssize_t result = compress_data(input, 1000, small_buffer, 10,
                                    COMPRESS_LEVEL_DEFAULT);
    ASSERT(result == COMPRESS_ERROR_BUFFER_TOO_SMALL, 
           "Should fail with buffer too small");
    
    printf("Correctly detected buffer too small\n");
    
    TEST_PASS();
}

/**
 * Test 8: Invalid Compressed Data
 */
void test_invalid_data(void) {
    TEST_START("Invalid Compressed Data");
    
    // Create invalid compressed data
    uint8_t invalid[100];
    memset(invalid, 0xFF, 100);
    
    uint8_t output[1000];
    ssize_t result = decompress_data(invalid, 100, output, 1000);
    
    ASSERT(result < 0, "Should fail to decompress invalid data");
    ASSERT(result == COMPRESS_ERROR_DECOMPRESS_FAILED, 
           "Should return decompress failed error");
    
    printf("Correctly rejected invalid compressed data\n");
    
    TEST_PASS();
}

/**
 * Test 9: Compression Ratio Calculation
 */
void test_compression_ratio(void) {
    TEST_START("Compression Ratio Calculation");
    
    // Test ratio calculation
    int ratio = compression_ratio(1000, 250);
    ASSERT(ratio == 75, "Should be 75% compression");
    
    ratio = compression_ratio(1000, 500);
    ASSERT(ratio == 50, "Should be 50% compression");
    
    ratio = compression_ratio(1000, 1000);
    ASSERT(ratio == 0, "Should be 0% (no compression)");
    
    ratio = compression_ratio(1000, 1100);
    ASSERT(ratio == 0, "Should be 0% (expanded)");
    
    printf("Compression ratio calculation validated\n");
    
    TEST_PASS();
}

/**
 * Test 10: Error Strings
 */
void test_error_strings(void) {
    TEST_START("Error Strings");
    
    const char *err = compression_error_string(COMPRESS_SUCCESS);
    ASSERT(strcmp(err, "Success") == 0, "Error string should match");
    
    err = compression_error_string(COMPRESS_ERROR_BUFFER_TOO_SMALL);
    ASSERT(strcmp(err, "Output buffer too small") == 0, "Error string should match");
    
    err = compression_error_string(COMPRESS_ERROR_DECOMPRESS_FAILED);
    ASSERT(strcmp(err, "Decompression failed") == 0, "Error string should match");
    
    printf("Error strings validated\n");
    
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║            Compression Unit Tests                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_compress_decompress();
    test_large_data();
    test_random_data();
    test_compression_levels();
    test_should_compress();
    test_edge_cases();
    test_buffer_too_small();
    test_invalid_data();
    test_compression_ratio();
    test_error_strings();
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ TEST SUMMARY                                               ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total tests: %2d                                            ║\n", tests_run);
    printf("║ Passed:      %2d                                            ║\n", tests_passed);
    printf("║ Failed:      %2d                                            ║\n", tests_run - tests_passed);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    if (tests_passed == tests_run) {
        printf("\n🎉 ALL TESTS PASSED! 🎉\n\n");
        return 0;
    } else {
        printf("\n❌ SOME TESTS FAILED ❌\n\n");
        return 1;
    }
}
