/**
 * multi_cache.h
 * 
 * Multi-slot Response Cache for ICMP Resource Interception
 * 
 * Features:
 * - 50-slot cache with request_id indexing
 * - Dynamic memory allocation for responses (up to 10 MB each)
 * - LRU (Least Recently Used) eviction when full
 * - 60-second expiration timeout
 * - Thread-safe with pthread mutex
 */

#ifndef MULTI_CACHE_H
#define MULTI_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>

// Cache configuration
#define CACHE_MAX_ENTRIES 50
#define CACHE_MAX_RESPONSE_SIZE (10 * 1024 * 1024)  // 10 MB
#define CACHE_ENTRY_TIMEOUT 60  // seconds
#define CACHE_FRAGMENT_SIZE 1200  // bytes per fragment

/**
 * Cache entry structure
 * Stores a single HTTP response indexed by request_id
 */
typedef struct {
    uint16_t request_id;           // Unique request identifier
    uint8_t* response_data;        // Dynamically allocated response data
    size_t response_size;          // Total size of response in bytes
    time_t timestamp;              // Entry creation time (for expiration)
    uint8_t in_use;                // Entry occupied flag (1=in use, 0=free)
} cache_entry_t;

/**
 * Multi-response cache structure
 * Thread-safe cache with mutex protection
 */
typedef struct {
    cache_entry_t entries[CACHE_MAX_ENTRIES];  // Fixed-size array of entries
    pthread_mutex_t lock;                       // Mutex for thread safety
} multi_response_cache_t;

/**
 * Initialize cache
 * 
 * @param cache Cache structure to initialize
 * 
 * Note: Must be called before any other cache operations
 *       Initializes mutex and sets all entries to unused
 * 
 * Example:
 *   multi_response_cache_t cache;
 *   cache_init(&cache);
 */
void cache_init(multi_response_cache_t* cache);

/**
 * Store response in cache
 * 
 * @param cache Cache structure
 * @param request_id Unique request identifier
 * @param data Response data to store
 * @param size Size of response data
 * 
 * @return 0 on success, -1 on error (e.g., size exceeds limit)
 * 
 * Note: If cache is full, evicts oldest entry (LRU)
 *       Allocates memory dynamically for response_data
 *       Thread-safe (acquires lock internally)
 * 
 * Example:
 *   uint8_t response[5000];
 *   cache_store(&cache, 42, response, 5000);
 */
int cache_store(multi_response_cache_t* cache, 
               uint16_t request_id,
               const uint8_t* data, 
               size_t size);

/**
 * Retrieve response from cache by request_id
 * 
 * @param cache Cache structure
 * @param request_id Request identifier to look up
 * 
 * @return Pointer to cache entry if found, NULL otherwise
 * 
 * Note: Returned pointer is valid until next cache operation
 *       Thread-safe (acquires lock internally)
 *       Checks expiration (60 seconds)
 * 
 * Example:
 *   cache_entry_t* entry = cache_get(&cache, 42);
 *   if (entry) {
 *       printf("Response size: %zu\n", entry->response_size);
 *   }
 */
cache_entry_t* cache_get(multi_response_cache_t* cache, 
                        uint16_t request_id);

/**
 * Retrieve fragment from cached response
 * 
 * @param cache Cache structure
 * @param request_id Request identifier
 * @param fragment_num Fragment number (0-based)
 * @param output Output buffer for fragment data
 * @param output_size Output: actual size of fragment
 * 
 * @return 0 on success, -1 if not found or invalid fragment_num
 * 
 * Note: Fragment size is 1200 bytes except last fragment
 *       Last fragment may be smaller than 1200 bytes
 *       Thread-safe (acquires lock internally)
 * 
 * Example:
 *   uint8_t fragment[1200];
 *   size_t frag_size;
 *   if (cache_get_fragment(&cache, 42, 2, fragment, &frag_size) == 0) {
 *       printf("Fragment 2: %zu bytes\n", frag_size);
 *   }
 */
int cache_get_fragment(multi_response_cache_t* cache,
                      uint16_t request_id,
                      uint8_t fragment_num,
                      uint8_t* output,
                      size_t* output_size);

/**
 * Evict oldest entry (LRU)
 * 
 * @param cache Cache structure
 * 
 * Note: Finds entry with oldest timestamp and frees it
 *       Called automatically by cache_store when cache is full
 *       Thread-safe (caller must hold lock)
 * 
 * Example:
 *   cache_evict_oldest(&cache);
 */
void cache_evict_oldest(multi_response_cache_t* cache);

/**
 * Cleanup expired entries
 * 
 * @param cache Cache structure
 * 
 * @return Number of entries removed
 * 
 * Note: Removes entries older than CACHE_ENTRY_TIMEOUT (60 seconds)
 *       Should be called periodically (e.g., every 10 seconds)
 *       Thread-safe (acquires lock internally)
 * 
 * Example:
 *   int removed = cache_cleanup_expired(&cache);
 *   printf("Removed %d expired entries\n", removed);
 */
int cache_cleanup_expired(multi_response_cache_t* cache);

/**
 * Free cache and all allocated memory
 * 
 * @param cache Cache structure
 * 
 * Note: Frees all response_data allocations and destroys mutex
 *       Call when cache is no longer needed
 * 
 * Example:
 *   cache_destroy(&cache);
 */
void cache_destroy(multi_response_cache_t* cache);

/**
 * Get cache statistics
 * 
 * @param cache Cache structure
 * @param used_entries Output: number of entries in use
 * @param total_size Output: total size of cached responses in bytes
 * 
 * Note: Thread-safe (acquires lock internally)
 * 
 * Example:
 *   size_t used, total;
 *   cache_get_stats(&cache, &used, &total);
 *   printf("Cache: %zu/%d entries, %zu bytes\n", used, CACHE_MAX_ENTRIES, total);
 */
void cache_get_stats(multi_response_cache_t* cache,
                    size_t* used_entries,
                    size_t* total_size);

#endif // MULTI_CACHE_H
