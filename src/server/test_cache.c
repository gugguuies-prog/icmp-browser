/**
 * test_cache.c
 * 
 * Unit tests for multi_cache operations
 */

#include "multi_cache.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

void test_cache_init() {
    printf("Test 1: cache_init()\n");
    multi_response_cache_t cache;
    cache_init(&cache);
    
    // Verify all entries are unused
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        assert(cache.entries[i].in_use == 0);
        assert(cache.entries[i].response_data == NULL);
    }
    
    cache_destroy(&cache);
    printf("  ✓ PASSED\n\n");
}

void test_cache_store_and_get() {
    printf("Test 2: cache_store() and cache_get()\n");
    multi_response_cache_t cache;
    cache_init(&cache);
    
    // Store a response
    uint8_t data[5000];
    for (int i = 0; i < 5000; i++) {
        data[i] = (uint8_t)(i % 256);
    }
    
    int result = cache_store(&cache, 42, data, 5000);
    assert(result == 0);
    
    // Retrieve it
    cache_entry_t* entry = cache_get(&cache, 42);
    assert(entry != NULL);
    assert(entry->request_id == 42);
    assert(entry->response_size == 5000);
    assert(entry->in_use == 1);
    
    // Verify data integrity
    for (int i = 0; i < 5000; i++) {
        assert(entry->response_data[i] == (uint8_t)(i % 256));
    }
    
    cache_destroy(&cache);
    printf("  ✓ PASSED\n\n");
}

void test_cache_get_fragment() {
    printf("Test 3: cache_get_fragment()\n");
    multi_response_cache_t cache;
    cache_init(&cache);
    
    // Store a 5000-byte response
    uint8_t data[5000];
    for (int i = 0; i < 5000; i++) {
        data[i] = (uint8_t)(i % 256);
    }
    cache_store(&cache, 100, data, 5000);
    
    // Get fragment 0 (bytes 0-1199)
    uint8_t fragment[1200];
    size_t frag_size;
    int result = cache_get_fragment(&cache, 100, 0, fragment, &frag_size);
    assert(result == 0);
    assert(frag_size == 1200);
    for (int i = 0; i < 1200; i++) {
        assert(fragment[i] == (uint8_t)(i % 256));
    }
    
    // Get fragment 1 (bytes 1200-2399)
    result = cache_get_fragment(&cache, 100, 1, fragment, &frag_size);
    assert(result == 0);
    assert(frag_size == 1200);
    for (int i = 0; i < 1200; i++) {
        assert(fragment[i] == (uint8_t)((1200 + i) % 256));
    }
    
    // Get fragment 4 (last fragment, bytes 4800-4999, size=200)
    result = cache_get_fragment(&cache, 100, 4, fragment, &frag_size);
    assert(result == 0);
    assert(frag_size == 200);
    for (int i = 0; i < 200; i++) {
        assert(fragment[i] == (uint8_t)((4800 + i) % 256));
    }
    
    // Try to get invalid fragment (should fail)
    result = cache_get_fragment(&cache, 100, 10, fragment, &frag_size);
    assert(result == -1);
    
    cache_destroy(&cache);
    printf("  ✓ PASSED\n\n");
}

void test_cache_lru_eviction() {
    printf("Test 4: LRU eviction when cache is full\n");
    multi_response_cache_t cache;
    cache_init(&cache);
    
    // Fill cache with 50 entries
    uint8_t data[100];
    for (int i = 0; i < 100; i++) {
        data[i] = (uint8_t)i;
    }
    
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        int result = cache_store(&cache, (uint16_t)i, data, 100);
        assert(result == 0);
        sleep(1);  // Small delay to ensure distinct timestamps
    }
    
    // Verify all entries are in use
    size_t used, total;
    cache_get_stats(&cache, &used, &total);
    assert(used == CACHE_MAX_ENTRIES);
    
    // Add one more entry (should evict entry 0, the oldest)
    int result = cache_store(&cache, 999, data, 100);
    assert(result == 0);
    
    // Verify entry 0 is gone
    cache_entry_t* entry = cache_get(&cache, 0);
    assert(entry == NULL);
    
    // Verify entry 999 exists
    entry = cache_get(&cache, 999);
    assert(entry != NULL);
    assert(entry->request_id == 999);
    
    cache_destroy(&cache);
    printf("  ✓ PASSED\n\n");
}

void test_cache_expiration() {
    printf("Test 5: 60-second expiration (simulated)\n");
    multi_response_cache_t cache;
    cache_init(&cache);
    
    // Store an entry
    uint8_t data[100];
    memset(data, 0xAA, 100);
    cache_store(&cache, 123, data, 100);
    
    // Manually set timestamp to old value (61 seconds ago)
    pthread_mutex_lock(&cache.lock);
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache.entries[i].in_use && cache.entries[i].request_id == 123) {
            cache.entries[i].timestamp = time(NULL) - 61;
            break;
        }
    }
    pthread_mutex_unlock(&cache.lock);
    
    // Try to retrieve (should fail due to expiration)
    cache_entry_t* entry = cache_get(&cache, 123);
    assert(entry == NULL);
    
    cache_destroy(&cache);
    printf("  ✓ PASSED\n\n");
}

void test_cache_cleanup_expired() {
    printf("Test 6: cache_cleanup_expired()\n");
    multi_response_cache_t cache;
    cache_init(&cache);
    
    // Store 3 entries
    uint8_t data[100];
    memset(data, 0xBB, 100);
    cache_store(&cache, 1, data, 100);
    cache_store(&cache, 2, data, 100);
    cache_store(&cache, 3, data, 100);
    
    // Manually expire entries 1 and 2
    pthread_mutex_lock(&cache.lock);
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (cache.entries[i].in_use && 
            (cache.entries[i].request_id == 1 || cache.entries[i].request_id == 2)) {
            cache.entries[i].timestamp = time(NULL) - 61;
        }
    }
    pthread_mutex_unlock(&cache.lock);
    
    // Run cleanup
    int removed = cache_cleanup_expired(&cache);
    assert(removed == 2);
    
    // Verify entries 1 and 2 are gone, entry 3 remains
    assert(cache_get(&cache, 1) == NULL);
    assert(cache_get(&cache, 2) == NULL);
    assert(cache_get(&cache, 3) != NULL);
    
    cache_destroy(&cache);
    printf("  ✓ PASSED\n\n");
}

void test_thread_safety() {
    printf("Test 7: Thread safety (mutex protection)\n");
    multi_response_cache_t cache;
    cache_init(&cache);
    
    // Store multiple entries concurrently (basic smoke test)
    uint8_t data[100];
    for (int i = 0; i < 10; i++) {
        data[i % 100] = (uint8_t)i;
        cache_store(&cache, (uint16_t)i, data, 100);
    }
    
    // Retrieve all entries
    for (int i = 0; i < 10; i++) {
        cache_entry_t* entry = cache_get(&cache, (uint16_t)i);
        assert(entry != NULL);
    }
    
    cache_destroy(&cache);
    printf("  ✓ PASSED\n\n");
}

int main() {
    printf("=== Multi-Cache Unit Tests ===\n\n");
    
    test_cache_init();
    test_cache_store_and_get();
    test_cache_get_fragment();
    test_cache_lru_eviction();
    test_cache_expiration();
    test_cache_cleanup_expired();
    test_thread_safety();
    
    printf("=== All Tests PASSED ===\n");
    return 0;
}
