/**
 * compression.c
 * 
 * Data Compression Implementation using zlib
 */

#include "compression.h"
#include <zlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Get maximum compressed size for given input size
 */
size_t compress_max_size(size_t input_size) {
    // zlib may expand data slightly for incompressible input
    // Use compressBound() for accurate estimate
    return compressBound((uLong)input_size);
}

/**
 * Compress data
 */
ssize_t compress_data(const uint8_t *input, size_t input_size,
                       uint8_t *output, size_t output_size, int level) {
    if (!input || !output || input_size == 0) {
        return COMPRESS_ERROR_INVALID_PARAM;
    }
    
    // Validate compression level
    if (level < 0 || level > 9) {
        level = COMPRESS_LEVEL_DEFAULT;
    }
    
    // Compress using zlib
    uLongf compressed_size = (uLongf)output_size;
    int result = compress2(output, &compressed_size, input, 
                           (uLong)input_size, level);
    
    if (result != Z_OK) {
        if (result == Z_BUF_ERROR) {
            return COMPRESS_ERROR_BUFFER_TOO_SMALL;
        } else if (result == Z_MEM_ERROR) {
            return COMPRESS_ERROR_MEMORY;
        } else {
            return COMPRESS_ERROR_COMPRESS_FAILED;
        }
    }
    
    return (ssize_t)compressed_size;
}

/**
 * Decompress data
 */
ssize_t decompress_data(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size) {
    if (!input || !output || input_size == 0) {
        return COMPRESS_ERROR_INVALID_PARAM;
    }
    
    // Decompress using zlib
    uLongf decompressed_size = (uLongf)output_size;
    int result = uncompress(output, &decompressed_size, input, (uLong)input_size);
    
    if (result != Z_OK) {
        if (result == Z_BUF_ERROR) {
            return COMPRESS_ERROR_BUFFER_TOO_SMALL;
        } else if (result == Z_MEM_ERROR) {
            return COMPRESS_ERROR_MEMORY;
        } else if (result == Z_DATA_ERROR) {
            return COMPRESS_ERROR_DECOMPRESS_FAILED;
        } else {
            return COMPRESS_ERROR_DECOMPRESS_FAILED;
        }
    }
    
    return (ssize_t)decompressed_size;
}

/**
 * Check if compression is recommended
 */
int should_compress(size_t data_size) {
    // Don't compress very small data (overhead not worth it)
    return (data_size >= COMPRESS_MIN_SIZE);
}

/**
 * Get compression ratio
 */
int compression_ratio(size_t original_size, size_t compressed_size) {
    if (original_size == 0) {
        return 0;
    }
    
    // Calculate percentage saved
    if (compressed_size >= original_size) {
        return 0;  // No compression (or expansion)
    }
    
    size_t saved = original_size - compressed_size;
    return (int)((saved * 100) / original_size);
}

/**
 * Get error message string
 */
const char* compression_error_string(int error_code) {
    switch (error_code) {
        case COMPRESS_SUCCESS:
            return "Success";
        case COMPRESS_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case COMPRESS_ERROR_BUFFER_TOO_SMALL:
            return "Output buffer too small";
        case COMPRESS_ERROR_COMPRESS_FAILED:
            return "Compression failed";
        case COMPRESS_ERROR_DECOMPRESS_FAILED:
            return "Decompression failed";
        case COMPRESS_ERROR_MEMORY:
            return "Memory allocation failed";
        default:
            return "Unknown error";
    }
}
