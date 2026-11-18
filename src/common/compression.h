/**
 * compression.h
 * 
 * Data Compression using zlib
 * 
 * Features:
 * - DEFLATE compression algorithm
 * - Automatic buffer sizing
 * - Compression level configuration
 * - Compress before encryption
 */

#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// Compression levels
#define COMPRESS_LEVEL_NONE 0            // No compression
#define COMPRESS_LEVEL_FAST 1            // Fastest compression
#define COMPRESS_LEVEL_DEFAULT 6         // Default (balanced)
#define COMPRESS_LEVEL_BEST 9            // Best compression

// Error codes
#define COMPRESS_SUCCESS 0
#define COMPRESS_ERROR_INVALID_PARAM -1
#define COMPRESS_ERROR_BUFFER_TOO_SMALL -2
#define COMPRESS_ERROR_COMPRESS_FAILED -3
#define COMPRESS_ERROR_DECOMPRESS_FAILED -4
#define COMPRESS_ERROR_MEMORY -5

// Minimum size to compress (below this, compression is not worth it)
#define COMPRESS_MIN_SIZE 64

/**
 * Get maximum compressed size for given input size
 * 
 * @param input_size Input data size
 * 
 * @return Maximum possible compressed size
 * 
 * Note: Compressed data can be slightly larger than input for incompressible data
 * 
 * Example:
 *   size_t max_size = compress_max_size(1000);
 *   uint8_t *compressed = malloc(max_size);
 */
size_t compress_max_size(size_t input_size);

/**
 * Compress data
 * 
 * @param input Input data to compress
 * @param input_size Size of input data
 * @param output Output buffer for compressed data
 * @param output_size Size of output buffer
 * @param level Compression level (1-9, or COMPRESS_LEVEL_*)
 * 
 * @return Compressed size on success, negative error code on failure
 * 
 * Note: If output buffer is too small, returns COMPRESS_ERROR_BUFFER_TOO_SMALL
 * 
 * Example:
 *   uint8_t input[1000] = "data to compress...";
 *   uint8_t output[1100];
 *   ssize_t compressed_size = compress_data(input, 1000, output, 1100, 
 *                                            COMPRESS_LEVEL_DEFAULT);
 *   if (compressed_size > 0) {
 *       printf("Compressed %zu bytes to %zd bytes\n", 1000, compressed_size);
 *   }
 */
ssize_t compress_data(const uint8_t *input, size_t input_size,
                       uint8_t *output, size_t output_size, int level);

/**
 * Decompress data
 * 
 * @param input Compressed input data
 * @param input_size Size of compressed data
 * @param output Output buffer for decompressed data
 * @param output_size Size of output buffer
 * 
 * @return Decompressed size on success, negative error code on failure
 * 
 * Note: Output buffer must be large enough for decompressed data
 * 
 * Example:
 *   uint8_t compressed[100];
 *   uint8_t decompressed[1000];
 *   ssize_t decompressed_size = decompress_data(compressed, 100, 
 *                                                decompressed, 1000);
 *   if (decompressed_size > 0) {
 *       printf("Decompressed to %zd bytes\n", decompressed_size);
 *   }
 */
ssize_t decompress_data(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size);

/**
 * Check if compression is recommended for given data size
 * 
 * @param data_size Data size in bytes
 * 
 * @return 1 if compression recommended, 0 otherwise
 * 
 * Note: Very small data (< 64 bytes) is not worth compressing
 * 
 * Example:
 *   if (should_compress(data_size)) {
 *       // Compress data
 *   } else {
 *       // Send uncompressed
 *   }
 */
int should_compress(size_t data_size);

/**
 * Get compression ratio (percentage)
 * 
 * @param original_size Original data size
 * @param compressed_size Compressed data size
 * 
 * @return Compression ratio as percentage (0-100)
 * 
 * Example:
 *   int ratio = compression_ratio(1000, 250);
 *   printf("Compression: %d%% (saved %zu bytes)\n", ratio, 1000 - 250);
 */
int compression_ratio(size_t original_size, size_t compressed_size);

/**
 * Get error message string
 * 
 * @param error_code Error code
 * @return Human-readable error message
 */
const char* compression_error_string(int error_code);

#endif // COMPRESSION_H
