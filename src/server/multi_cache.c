/**
 * multi_cache.c
 * 
 * Implementation of Multi-slot Response Cache
 */

#include "multi_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Initialize cache
 */
void cache_init(multi_response_cache_t* cache) {
    if (!cache) return;
    
    // Initialize mutex
    pthread_mutex_init(&cache->lock, NULL);
    
    // Clear all entries
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        cache->entries[i].request_id = 0;
        cache->entries[i].response_data = NULL;
        cache->entries[i].response_size = 0;
        cache->entries[i].timestamp = 0;
        cache->entries[i].in_use = 0;
    }
}

/**
 * Store response in cache
 */
int cache_store(multi_response_cache_t* cache, 
               uint16_t request_id,
               const uint8_t* data, 
               size_t size) {
    if (!cache || !data || size == 0) {
        return -1;
    }
    
    // Check size limit
    if (size > CACHE_MAX_RESPONSE_SIZE) {
        fprintf(stderr, "[CACHE] Response size %zu exceeds limit %d\n", 
                size, CACHE_MAX_RESPONSE_SIZE);
        return -1;
    }
    
    pthread_mutex_lock(&cache->lock);
    
    // Check if request_id already exists (update case)
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache->entries[i].in_use && 
            cache->entries[i].request_id == request_id) {
            // Update existing entry
            free(cache->entries[i].response_data);
            cache->entries[i].response_data = (uint8_t*)malloc(size);
            if (!cache->entries[i].response_data) {
                cache->entries[i].in_use = 0;
                pthread_mutex_unlock(&cache->lock);
                return -1;
            }
            memcpy(cache->entries[i].response_data, data, size);
            cache->entries[i].response_size = size;
            cache->entries[i].timestamp = time(NULL);
            pthread_mutex_unlock(&cache->lock);
            return 0;
        }
    }
    
    // Find free slot
    int free_slot = -1;
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (!cache->entries[i].in_use) {
            free_slot = i;
            break;
        }
    }
    
    // If no free slot, evict oldest
    if (free_slot == -1) {
        cache_evict_oldest(cache);
        
        // Find free slot after eviction
        for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
            if (!cache->entries[i].in_use) {
                free_slot = i;
                break;
            }
        }
    }
    
    if (free_slot == -1) {
        // Should never happen
        fprintf(stderr, "[CACHE] ERROR: No free slot after eviction\n");
        pthread_mutex_unlock(&cache->lock);
        return -1;
    }
    
    // Allocate and store
    cache->entries[free_slot].response_data = (uint8_t*)malloc(size);
    if (!cache->entries[free_slot].response_data) {
        fprintf(stderr, "[CACHE] ERROR: Failed to allocate %zu bytes\n", size);
        pthread_mutex_unlock(&cache->lock);
        return -1;
    }
    
    memcpy(cache->entries[free_slot].response_data, data, size);
    cache->entries[free_slot].response_size = size;
    cache->entries[free_slot].request_id = request_id;
    cache->entries[free_slot].timestamp = time(NULL);
    cache->entries[free_slot].in_use = 1;
    
    fprintf(stderr, "[CACHE] Stored request_id=%u, size=%zu bytes in slot %d\n",
            request_id, size, free_slot);
    
    pthread_mutex_unlock(&cache->lock);
    return 0;
}

/**
 * Retrieve response from cache
 */
cache_entry_t* cache_get(multi_response_cache_t* cache, 
                        uint16_t request_id) {
    if (!cache) return NULL;
    
    pthread_mutex_lock(&cache->lock);
    
    time_t now = time(NULL);
    
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache->entries[i].in_use && 
            cache->entries[i].request_id == request_id) {
            
            // Check expiration
            if (now - cache->entries[i].timestamp > CACHE_ENTRY_TIMEOUT) {
                fprintf(stderr, "[CACHE] Request_id=%u expired\n", request_id);
                // Free expired entry
                free(cache->entries[i].response_data);
                cache->entries[i].response_data = NULL;
                cache->entries[i].in_use = 0;
                pthread_mutex_unlock(&cache->lock);
                return NULL;
            }
            
            pthread_mutex_unlock(&cache->lock);
            return &cache->entries[i];
        }
    }
    
    pthread_mutex_unlock(&cache->lock);
    return NULL;
}

/**
 * Retrieve fragment from cached response
 */
int cache_get_fragment(multi_response_cache_t* cache,
                      uint16_t request_id,
                      uint8_t fragment_num,
                      uint8_t* output,
                      size_t* output_size) {
    if (!cache || !output || !output_size) {
        return -1;
    }
    
    pthread_mutex_lock(&cache->lock);
    
    // Find entry
    cache_entry_t* entry = NULL;
    time_t now = time(NULL);
    
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache->entries[i].in_use && 
            cache->entries[i].request_id == request_id) {
            
            // Check expiration
            if (now - cache->entries[i].timestamp > CACHE_ENTRY_TIMEOUT) {
                fprintf(stderr, "[CACHE] Request_id=%u expired\n", request_id);
                free(cache->entries[i].response_data);
                cache->entries[i].response_data = NULL;
                cache->entries[i].in_use = 0;
                pthread_mutex_unlock(&cache->lock);
                return -1;
            }
            
            entry = &cache->entries[i];
            break;
        }
    }
    
    if (!entry) {
        pthread_mutex_unlock(&cache->lock);
        return -1;
    }
    
    // Calculate fragment offset and size
    size_t offset = (size_t)fragment_num * CACHE_FRAGMENT_SIZE;
    
    if (offset >= entry->response_size) {
        fprintf(stderr, "[CACHE] Fragment %u out of range (offset=%zu, size=%zu)\n",
                fragment_num, offset, entry->response_size);
        pthread_mutex_unlock(&cache->lock);
        return -1;
    }
    
    size_t remaining = entry->response_size - offset;
    size_t fragment_size = (remaining > CACHE_FRAGMENT_SIZE) ? 
                           CACHE_FRAGMENT_SIZE : remaining;
    
    memcpy(output, entry->response_data + offset, fragment_size);
    *output_size = fragment_size;
    
    fprintf(stderr, "[CACHE] Retrieved fragment %u for request_id=%u: %zu bytes\n",
            fragment_num, request_id, fragment_size);
    
    pthread_mutex_unlock(&cache->lock);
    return 0;
}

/**
 * Evict oldest entry (LRU)
 * Note: Caller must hold lock
 */
void cache_evict_oldest(multi_response_cache_t* cache) {
    if (!cache) return;
    
    int oldest_idx = -1;
    time_t oldest_time = 0;
    
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache->entries[i].in_use) {
            if (oldest_idx == -1 || 
                cache->entries[i].timestamp < oldest_time) {
                oldest_idx = i;
                oldest_time = cache->entries[i].timestamp;
            }
        }
    }
    
    if (oldest_idx != -1) {
        fprintf(stderr, "[CACHE] Evicting oldest entry: request_id=%u, age=%ld sec\n",
                cache->entries[oldest_idx].request_id,
                (long)(time(NULL) - oldest_time));
        
        free(cache->entries[oldest_idx].response_data);
        cache->entries[oldest_idx].response_data = NULL;
        cache->entries[oldest_idx].in_use = 0;
    }
}

/**
 * Cleanup expired entries
 */
int cache_cleanup_expired(multi_response_cache_t* cache) {
    if (!cache) return 0;
    
    pthread_mutex_lock(&cache->lock);
    
    int removed = 0;
    time_t now = time(NULL);
    
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache->entries[i].in_use) {
            if (now - cache->entries[i].timestamp > CACHE_ENTRY_TIMEOUT) {
                fprintf(stderr, "[CACHE] Removing expired entry: request_id=%u\n",
                        cache->entries[i].request_id);
                free(cache->entries[i].response_data);
                cache->entries[i].response_data = NULL;
                cache->entries[i].in_use = 0;
                removed++;
            }
        }
    }
    
    pthread_mutex_unlock(&cache->lock);
    return removed;
}

/**
 * Free cache and all allocated memory
 */
void cache_destroy(multi_response_cache_t* cache) {
    if (!cache) return;
    
    pthread_mutex_lock(&cache->lock);
    
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache->entries[i].response_data) {
            free(cache->entries[i].response_data);
            cache->entries[i].response_data = NULL;
        }
        cache->entries[i].in_use = 0;
    }
    
    pthread_mutex_unlock(&cache->lock);
    pthread_mutex_destroy(&cache->lock);
}

/**
 * Get cache statistics
 */
void cache_get_stats(multi_response_cache_t* cache,
                    size_t* used_entries,
                    size_t* total_size) {
    if (!cache) return;
    
    pthread_mutex_lock(&cache->lock);
    
    size_t used = 0;
    size_t total = 0;
    
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache->entries[i].in_use) {
            used++;
            total += cache->entries[i].response_size;
        }
    }
    
    if (used_entries) *used_entries = used;
    if (total_size) *total_size = total;
    
    pthread_mutex_unlock(&cache->lock);
}
